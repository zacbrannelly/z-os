#include "mmap.h"
#include "bump_allocator.h"
#include "format.h"
#include "console.h"
#include "math.h"

#include <stddef.h>

// G = Gathering, R = Reordering, E = Early Write Acknowledgement
#define MAIR_ATTR_DEVICE_MEMORY_MASK 0xF3ULL // 0b11110011
#define MAIR_ATTR_DEVICE_NGNRNE 0x0
#define MAIR_ATTR_DEVICE_NGNRE 0x1
#define MAIR_ATTR_DEVICE_NGRE 0x2
#define MAIR_ATTR_DEVICE_GRE 0x3
#define MAIR_ATTR_DEVICE_TYPE_SHIFT 2
#define MAIR_ATTR_DEVICE_TYPE_MASK 0x3ULL
#define MAIR_ATTR_DEVICE_TYPE(attr) ((attr >> MAIR_ATTR_DEVICE_TYPE_SHIFT) & MAIR_ATTR_DEVICE_TYPE_MASK)

// Cache policy: WB = Write-Back (write to memory when needed), WT = Write-Through (write to cache and memory), NC = Non-Cacheable (no caching)
// Transient = Hint to the CPU that it only needs to be cached for a short period of time.
// Cache line allocation hints: R = Read Only, W = Write Only, RW = Read/Write
#define MAIR_ATTR_NORMAL_WT_TRANSIENT_R  0x02  // 0b0010
#define MAIR_ATTR_NORMAL_WT_TRANSIENT_W  0x01  // 0b0001
#define MAIR_ATTR_NORMAL_WT_TRANSIENT_RW 0x03  // 0b0011
#define MAIR_ATTR_NORMAL_NC              0x04  // 0b0100
#define MAIR_ATTR_NORMAL_WB_TRANSIENT_R  0x06  // 0b0110
#define MAIR_ATTR_NORMAL_WB_TRANSIENT_W  0x05  // 0b0101
#define MAIR_ATTR_NORMAL_WB_TRANSIENT_RW 0x07  // 0b0111
#define MAIR_ATTR_NORMAL_WT_R            0x0a  // 0b1010
#define MAIR_ATTR_NORMAL_WT_W            0x09  // 0b1001
#define MAIR_ATTR_NORMAL_WT_RW           0x0b  // 0b1011
#define MAIR_ATTR_NORMAL_WB_R            0x0e  // 0b1110
#define MAIR_ATTR_NORMAL_WB_W            0x0c  // 0b1101
#define MAIR_ATTR_NORMAL_WB_RW           0x0f  // 0b1111

#define MAIR_ATTR(reg, index) ((reg >> ((index) * 8)) & 0xFFULL)

#define TCR_TTBR0_ENABLE_FLAG ~(1ULL << 7)     // EPD0 flag must be clear to enable TTBR0 translations.
#define TCR_TTBR0_4KB_GRANULE_FLAG (0ULL << 15)      // TG0 bits must be set to 0x0 to indicate 4kb granule size.
#define TCR_TTBR0_CLEAR_GRANULE_FLAG ~(0x3ULL << 14) // TG0 bits must be cleared to set the desired value.
#define TCR_TTBR0_SET_T1SZ_16_FLAG (16ULL << 0)      // T0SZ bits must be set to 16 to indicate a 48 bit address space (64 - 16 = 48).
#define TCR_TTBR0_CLEAR_T1SZ_FLAG ~(0x3FULL << 0)    // T0SZ bits must be cleared to set the desired value.

#define TCR_TTBR1_ENABLE_FLAG ~(1ULL << 23)    // EPD1 flag must be clear to enable TTBR1 translations.
#define TCR_TTBR1_4KB_GRANULE_FLAG (1ULL << 31)      // TG1 bits must be set to 0x2 to indicate 4kb granule size.
#define TCR_TTBR1_CLEAR_GRANULE_FLAG ~(0x3ULL << 30) // TG1 bits must be cleared to set the desired value.
#define TCR_TTBR1_SET_T1SZ_16_FLAG (16ULL << 16)     // T1SZ bits must be set to 16 to indicate a 48 bit address space (64 - 16 = 48).
#define TCR_TTBR1_CLEAR_T1SZ_FLAG ~(0x3FULL << 16)   // T1SZ bits must be cleared to set the desired value.

static uint64_t* g_ttbr0_table = NULL;
static uint64_t* g_ttbr1_table = NULL;

// Simple bump allocator for the page tables, we'll use PAGE_TABLE_RESERVED_SIZE bytes of memory for it.
static bump_allocator_t g_page_table_allocator;
static efi_memory_descriptor_t *g_page_table_descriptor = NULL;

// Simple bump allocator for the kernel memory map, we'll use MMAP_MEMORY_MAP_RESERVED_SIZE bytes of memory for it.
static bump_allocator_t g_kernel_memory_map_allocator;
static uint64_t g_kernel_memory_map_count = 0;
static efi_memory_descriptor_t *g_kernel_memory_map_descriptor = NULL;

static uint64_t read_mair_el1(void) {
    uint64_t mair_el1 = 0;
    __asm__("mrs %0, mair_el1" : "=r" (mair_el1));
    return mair_el1;
}

static uint64_t read_ttbr0(void) {
    uint64_t ttbr0 = 0;
    __asm__("mrs %0, ttbr0_el1" : "=r" (ttbr0));
    return ttbr0;
}

static uint64_t read_ttbr1(void) {
    uint64_t ttbr1 = 0;
    __asm__("mrs %0, ttbr1_el1" : "=r" (ttbr1));
    return ttbr1;
}

static uint64_t read_tcr(void) {
    uint64_t tcr = 0;
    __asm__("mrs %0, tcr_el1" : "=r" (tcr));
    return tcr;
}

static uint64_t **get_ttbr_table_ptr(uint64_t virtual_address) {
    return virtual_address >= 0xFFFF000000000000 
        ? &g_ttbr1_table 
        : &g_ttbr0_table;
}

static void print_normal_mair_attributes(uint8_t attr) {
    switch (attr & 0x0f) {
        case MAIR_ATTR_NORMAL_WT_TRANSIENT_R:
            console_write(" - WT Transient R");
            break;
        case MAIR_ATTR_NORMAL_WT_TRANSIENT_W:
            console_write(" - WT Transient W");
            break;
        case MAIR_ATTR_NORMAL_WT_TRANSIENT_RW:
            console_write(" - WT Transient RW");
            break;
        case MAIR_ATTR_NORMAL_NC:
            console_write(" - NC");
            break;
        case MAIR_ATTR_NORMAL_WB_TRANSIENT_R:
            console_write(" - WB Transient R");
            break;
        case MAIR_ATTR_NORMAL_WB_TRANSIENT_W:
            console_write(" - WB Transient W");
            break;
        case MAIR_ATTR_NORMAL_WB_TRANSIENT_RW:
            console_write(" - WB Transient RW");
            break;
        case MAIR_ATTR_NORMAL_WT_R:
            console_write(" - WT R");
            break;
        case MAIR_ATTR_NORMAL_WT_W:
            console_write(" - WT W");
            break;
        case MAIR_ATTR_NORMAL_WT_RW:
            console_write(" - WT RW");
            break;
        case MAIR_ATTR_NORMAL_WB_R:
            console_write(" - WB R");
            break;
        case MAIR_ATTR_NORMAL_WB_W:
            console_write(" - WB W");
            break;
        case MAIR_ATTR_NORMAL_WB_RW:
            console_write(" - WB RW");
            break;
        default:
            console_write(" - Disabled");
            break;
    }
}

static void print_current_mair_attributes(void) {
    uint64_t mair_el1 = read_mair_el1();
    console_write("MAIR_EL1: ");
    console_write_hex(mair_el1);
    console_write("\r\n");

    for (int i = 0; i < 8; i++) {
        uint8_t attr = MAIR_ATTR(mair_el1, i);

        console_write("MAIR_EL1[");
        console_write_hex(i);
        console_write("]: ");
        console_write_hex((uint64_t)attr);
        console_write(" - ");

        if ((attr & MAIR_ATTR_DEVICE_MEMORY_MASK) == 0) {
            // Device memory
            console_write("Device memory");

            switch (MAIR_ATTR_DEVICE_TYPE(attr)) {
                case MAIR_ATTR_DEVICE_NGNRNE:
                    console_write(" - NGNRNE");
                    break;
                case MAIR_ATTR_DEVICE_NGNRE:
                    console_write(" - NGNRE");
                    break;
                case MAIR_ATTR_DEVICE_NGRE:
                    console_write(" - NGRE");
                    break;
                case MAIR_ATTR_DEVICE_GRE:
                    console_write(" - GRE");
                    break;
                default:
                    console_write(" - Unknown");
                    break;
            }
        } else {
            // Normal memory
            console_write("Normal memory");

            console_write(" - Inner Sharability");
            print_normal_mair_attributes(attr & 0x0f);

            console_write(" - Outer Sharability");
            print_normal_mair_attributes(attr >> 4);
        }

        console_write("\r\n");
    }
}

static void print_ttbr0_table(void) {
    // TODO: QUICK AI SLOP TO RENDER THE TABLE, REFACTOR OR REMOVE THIS IN FUTURE.
    #define PTE_VALID           (1ULL << 0)
    #define PTE_TABLE_OR_PAGE   (1ULL << 1)
    #define PTE_ADDR_MASK       0x0000FFFFFFFFF000ULL
    #define ENTRIES             512ULL

    typedef struct {
        uint64_t *table;
        int level;
        uint64_t va_base;
        uint64_t index;
    } stack_entry_t;

    uint64_t ttbr0 = read_ttbr0();
    uint64_t *root = (uint64_t *)(ttbr0 & PTE_ADDR_MASK);

    console_write("TTBR0: ");
    console_write_hex(ttbr0);
    console_write("\r\n");

    stack_entry_t stack[4];
    int sp = 0;

    stack[0].table = root;
    stack[0].level = 0;
    stack[0].va_base = 0;
    stack[0].index = 0;

    uint64_t current_va_start = 0;
    uint64_t current_va_end   = 0; // exclusive
    uint64_t current_pa_start = 0;
    uint64_t current_attrs    = 0;
    int have_current = 0;

    while (sp >= 0) {
        if (stack[sp].index >= ENTRIES) {
            sp--;
            continue;
        }

        uint64_t i       = stack[sp].index++;
        int level        = stack[sp].level;
        uint64_t *table  = stack[sp].table;
        uint64_t va_base = stack[sp].va_base;
        uint64_t pte     = table[i];

        if ((pte & PTE_VALID) == 0) {
            continue;
        }

        uint64_t span;
        switch (level) {
            case 0: span = 512ULL * 1024 * 1024 * 1024; break;
            case 1: span =   1ULL * 1024 * 1024 * 1024; break;
            case 2: span =   2ULL * 1024 * 1024; break;
            case 3: span =   4ULL * 1024; break;
            default: span = 0; break;
        }

        uint64_t entry_va = va_base + i * span;

        // L0/L1/L2: bit1=1 => next-level table
        // L3: bit1=1 => page mapping
        if (level < 3 && (pte & PTE_TABLE_OR_PAGE)) {
            sp++;
            stack[sp].table = (uint64_t *)(pte & PTE_ADDR_MASK);
            stack[sp].level = level + 1;
            stack[sp].va_base = entry_va;
            stack[sp].index = 0;
            continue;
        }

        uint64_t pa = pte & PTE_ADDR_MASK;

        // Merge signature
        uint64_t attrs =
            (((pte >> 2)  & 0x7)  << 0) |   // AttrIndx
            (((pte >> 6)  & 0x3)  << 3) |   // AP
            (((pte >> 8)  & 0x3)  << 5) |   // SH
            (((pte >> 10) & 0x1)  << 7) |   // AF
            (((pte >> 53) & 0x1)  << 8) |   // PXN
            (((pte >> 54) & 0x1)  << 9);    // UXN

        if (!have_current) {
            current_va_start = entry_va;
            current_va_end   = entry_va + span;
            current_pa_start = pa;
            current_attrs    = attrs;
            have_current = 1;
            continue;
        }

        uint64_t current_pa_end = current_pa_start + (current_va_end - current_va_start);

        if (current_va_end == entry_va &&
            current_pa_end == pa &&
            current_attrs == attrs) {
            current_va_end += span;
            continue;
        }

        {
            uint64_t attrindx = (current_attrs >> 0) & 0x7;
            uint64_t ap       = (current_attrs >> 3) & 0x3;
            uint64_t sh       = (current_attrs >> 5) & 0x3;
            uint64_t af       = (current_attrs >> 7) & 0x1;
            uint64_t pxn      = (current_attrs >> 8) & 0x1;
            uint64_t uxn      = (current_attrs >> 9) & 0x1;

            console_write("VA ");
            console_write_hex(current_va_start);
            console_write(" - ");
            console_write_hex(current_va_end - 1);
            console_write("  -> PA ");
            console_write_hex(current_pa_start);
            console_write("  ");

            switch (ap) {
                case 0: console_write("RW EL1 "); break;
                case 1: console_write("RW EL0 "); break;
                case 2: console_write("RO EL1 "); break;
                case 3: console_write("RO EL0 "); break;
                default: console_write("AP? "); break;
            }

            if (pxn && uxn) {
                console_write("NX ");
            } else if (!pxn && !uxn) {
                console_write("X ");
            } else {
                if (pxn) console_write("PXN ");
                if (uxn) console_write("UXN ");
            }

            switch (attrindx) {
                case 0: console_write("DEVICE-nGnRnE "); break;
                case 1: console_write("NORMAL-NC "); break;
                case 2: console_write("NORMAL-WT "); break;
                case 3: console_write("NORMAL-WB "); break;
                default:
                    console_write("ATTRIDX=");
                    console_write_hex(attrindx);
                    console_write(" ");
                    break;
            }

            switch (sh) {
                case 0: console_write("NON-SH "); break;
                case 2: console_write("OUTER-SH "); break;
                case 3: console_write("INNER-SH "); break;
                default: console_write("SH? "); break;
            }

            if (af) {
                console_write("AF ");
            } else {
                console_write("!AF ");
            }

            console_write("\r\n");
        }

        current_va_start = entry_va;
        current_va_end   = entry_va + span;
        current_pa_start = pa;
        current_attrs    = attrs;
    }

    if (have_current) {
        uint64_t attrindx = (current_attrs >> 0) & 0x7;
        uint64_t ap       = (current_attrs >> 3) & 0x3;
        uint64_t sh       = (current_attrs >> 5) & 0x3;
        uint64_t af       = (current_attrs >> 7) & 0x1;
        uint64_t pxn      = (current_attrs >> 8) & 0x1;
        uint64_t uxn      = (current_attrs >> 9) & 0x1;

        console_write("VA ");
        console_write_hex(current_va_start);
        console_write(" - ");
        console_write_hex(current_va_end - 1);
        console_write("  -> PA ");
        console_write_hex(current_pa_start);
        console_write("  ");

        switch (ap) {
            case 0: console_write("RW EL1 "); break;
            case 1: console_write("RW EL0 "); break;
            case 2: console_write("RO EL1 "); break;
            case 3: console_write("RO EL0 "); break;
            default: console_write("AP? "); break;
        }

        if (pxn && uxn) {
            console_write("NX ");
        } else if (!pxn && !uxn) {
            console_write("X ");
        } else {
            if (pxn) console_write("PXN ");
            if (uxn) console_write("UXN ");
        }

        switch (attrindx) {
            case 0: console_write("DEVICE-nGnRnE "); break;
            case 1: console_write("NORMAL-NC "); break;
            case 2: console_write("NORMAL-WT "); break;
            case 3: console_write("NORMAL-WB "); break;
            default:
                console_write("ATTRIDX=");
                console_write_hex(attrindx);
                console_write(" ");
                break;
        }

        switch (sh) {
            case 0: console_write("NON-SH "); break;
            case 2: console_write("OUTER-SH "); break;
            case 3: console_write("INNER-SH "); break;
            default: console_write("SH? "); break;
        }

        if (af) {
            console_write("AF ");
        } else {
            console_write("!AF ");
        }

        console_write("\r\n");
    }

    #undef PTE_VALID
    #undef PTE_TABLE_OR_PAGE
    #undef PTE_ADDR_MASK
    #undef ENTRIES
}

static int alloc_new_table(uint64_t **new_table) {
    if (bump_allocator_invalid(&g_page_table_allocator)) {
        console_write("Unable to allocate new TTBR0 table, no reserved page table memory available\r\n");
        return -1;
    }

    uint64_t new_page_table_address = 0;
    if (bump_allocator_allocate(&g_page_table_allocator, MMAP_GRANULE_SIZE, &new_page_table_address) < 0) {
        console_write("Unable to allocate new TTBR0 table, no pages left in the reserved page table memory\r\n");
        return -1;
    }

    // Zero the new table.
    uint64_t *new_page_table_base_addr = (uint64_t *)new_page_table_address;
    for (int i = 0; i < MMAP_GRANULE_SIZE / sizeof(uint64_t); i++) {
        new_page_table_base_addr[i] = 0;
    }

    // Set the new table pointer.
    *new_table = new_page_table_base_addr;
    return 0;
}

static int mmap_reserve_memory(
    efi_memory_descriptor_t *efi_memory_map,
    uint64_t efi_memory_map_size,
    uint64_t efi_memory_map_descriptor_size
) {
    // Set the bump allocators to invalid.
    bump_allocator_make_invalid(&g_kernel_memory_map_allocator);
    bump_allocator_make_invalid(&g_page_table_allocator);

    // Reserve page table memory and kernel memory map memory using the provided UEFI memory map.
    for (int i = 0; i < efi_memory_map_size; i += efi_memory_map_descriptor_size) {
        efi_memory_descriptor_t *descriptor = (efi_memory_descriptor_t *)((uint64_t)efi_memory_map + i);
        if (!efi_memory_map_is_usable(descriptor)) continue;

        uint64_t memory_start = descriptor->physical_start_address;
        uint64_t memory_size = descriptor->number_of_pages * EFI_PAGE_SIZE;

        int has_page_table_allocator = !bump_allocator_invalid(&g_page_table_allocator);
        int has_kernel_memory_map_allocator = !bump_allocator_invalid(&g_kernel_memory_map_allocator);

        // Attempt to reserve kernel memory map memory.
        if (!has_kernel_memory_map_allocator && memory_size >= MMAP_MEMORY_MAP_RESERVED_SIZE) {
            int result = bump_allocator_init(
                &g_kernel_memory_map_allocator,
                memory_start,
                memory_start + MMAP_MEMORY_MAP_RESERVED_SIZE
            );
            if (result < 0) {
                console_write("Failed to initialize kernel memory map allocator\r\n");
                return -1;
            }
            memory_start += MMAP_MEMORY_MAP_RESERVED_SIZE;
            memory_size -= MMAP_MEMORY_MAP_RESERVED_SIZE;
            g_kernel_memory_map_descriptor = descriptor;
        }

        // Attempt to reserve page table memory.
        if (!has_page_table_allocator && memory_size >= PAGE_TABLE_RESERVED_SIZE) {
            int result = bump_allocator_init(
                &g_page_table_allocator,
                memory_start,
                memory_start + PAGE_TABLE_RESERVED_SIZE
            );
            if (result < 0) {
                console_write("Failed to initialize page table allocator\r\n");
                return -1;
            }
            memory_start += PAGE_TABLE_RESERVED_SIZE;
            memory_size -= PAGE_TABLE_RESERVED_SIZE;
            g_page_table_descriptor = descriptor;
        }

        if (has_page_table_allocator && has_kernel_memory_map_allocator) {
            break;
        }
    }

    int has_page_table_allocator = !bump_allocator_invalid(&g_page_table_allocator);
    int has_kernel_memory_map_allocator = !bump_allocator_invalid(&g_kernel_memory_map_allocator);

    if (!has_page_table_allocator || !has_kernel_memory_map_allocator) {
        console_write("No conventional memory could be found during mmap initialization\r\n");
        return -1;
    }

    return 0;
}

static int mmap_new_memory_map_entry(
    uint64_t physical_start_address,
    uint64_t number_of_pages,
    mmap_memory_type_t type,
    mmap_memory_descriptor_t **out_new_entry
) {
    uint64_t new_entry_address = 0;
    if (bump_allocator_allocate(&g_kernel_memory_map_allocator, sizeof(mmap_memory_descriptor_t), &new_entry_address) < 0) {
        console_write("Failed to allocate new memory map entry\r\n");
        return -1;
    }

    mmap_memory_descriptor_t *new_entry = (mmap_memory_descriptor_t *)new_entry_address;
    new_entry->physical_start_address = physical_start_address;
    new_entry->number_of_pages = number_of_pages;
    new_entry->type = type;

    *out_new_entry = new_entry;
    g_kernel_memory_map_count++;
    return 0;
}

static int mmap_build_memory_map(
    efi_memory_descriptor_t *efi_memory_map,
    uint64_t efi_memory_map_size,
    uint64_t efi_memory_map_descriptor_size
) {
    for (int i = 0; i < efi_memory_map_size; i += efi_memory_map_descriptor_size) {
        efi_memory_descriptor_t *descriptor = (efi_memory_descriptor_t *)((uint64_t)efi_memory_map + i);
        uint64_t memory_start = descriptor->physical_start_address;
        uint64_t number_of_pages = descriptor->number_of_pages;
        uint64_t memory_end = memory_start + number_of_pages * EFI_PAGE_SIZE;

        if (efi_memory_map_is_usable(descriptor)) {
            int contains_kernel_memory_map_region = (
                g_kernel_memory_map_allocator.memory_start >= memory_start &&
                g_kernel_memory_map_allocator.memory_end <= memory_end
            );
            int contains_page_table_region = (
                g_page_table_allocator.memory_start >= memory_start &&
                g_page_table_allocator.memory_end <= memory_end
            );

            if (contains_kernel_memory_map_region) {
                // Allocate a map entry for the reserved kernel memory map region.
                mmap_memory_descriptor_t *new_entry = NULL;
                uint64_t kmap_start = g_kernel_memory_map_allocator.memory_start;
                uint64_t kmap_pages = MMAP_MEMORY_MAP_RESERVED_SIZE / EFI_PAGE_SIZE;
                if (mmap_new_memory_map_entry(kmap_start, kmap_pages, MMAP_MEMORY_TYPE_RESERVED, &new_entry) < 0) {
                    console_write("Failed to create new memory map entry for the kernel memory map region\r\n");
                    return -1;
                }

                memory_start += MMAP_MEMORY_MAP_RESERVED_SIZE;
                number_of_pages -= MMAP_MEMORY_MAP_RESERVED_SIZE / EFI_PAGE_SIZE;
            }

            if (contains_page_table_region) {
                // Allocate a map entry for the reserved page table region.
                mmap_memory_descriptor_t *new_entry = NULL;
                uint64_t ptable_start = g_page_table_allocator.memory_start;
                uint64_t ptable_pages = PAGE_TABLE_RESERVED_SIZE / EFI_PAGE_SIZE;
                if (mmap_new_memory_map_entry(ptable_start, ptable_pages, MMAP_MEMORY_TYPE_RESERVED, &new_entry) < 0) {
                    console_write("Failed to create new memory map entry for the page table region\r\n");
                    return -1;
                }

                memory_start += PAGE_TABLE_RESERVED_SIZE;
                number_of_pages -= PAGE_TABLE_RESERVED_SIZE / EFI_PAGE_SIZE;
            }

            // Allocate a map entry for the usable memory region.
            mmap_memory_descriptor_t *new_entry = NULL;
            if (mmap_new_memory_map_entry(memory_start, number_of_pages, MMAP_MEMORY_TYPE_USABLE, &new_entry) < 0) {
                console_write("Failed to create new memory map entry\r\n");
                return -1;
            }
        } else {
            // Allocate a map entry for the unusable memory region.
            mmap_memory_descriptor_t *new_entry = NULL;
            if (mmap_new_memory_map_entry(memory_start, number_of_pages, MMAP_MEMORY_TYPE_RESERVED, &new_entry) < 0) {
                console_write("Failed to create new memory map entry for the unusable memory region\r\n");
                return -1;
            }
        }
    }

    return 0;
}

int mmap_get_memory_map(
    mmap_memory_descriptor_t **out_memory_map,
    uint64_t *out_memory_map_count
) {
    if (bump_allocator_invalid(&g_kernel_memory_map_allocator)) {
        console_write("Failed to get memory map, kernel memory map allocator is invalid\r\n");
        return -1;
    }

    *out_memory_map = (mmap_memory_descriptor_t *)g_kernel_memory_map_allocator.memory_start;
    *out_memory_map_count = g_kernel_memory_map_count;
    return 0;
}

int mmap_build_ttbr0_table() {
    // Allocate the root table (first 512 entries).
    if (alloc_new_table(&g_ttbr0_table) < 0) {
        return -1;
    }

    // TODO: This should be mapped by the device driver instead of mmap directly.
    // NOTE: The PL011 UART is within this range!!!!!!
    // VA 0x4000000 - 0x3effffff  -> PA 0x4000000  RW EL1 NX DEVICE-nGnRnE NON-SH AF 
    if (mmap_map_range_l2_block(
        0x4000000,
        0x3effffff,
        0x4000000,
        PAGE_FLAG_EL1_RW |
        PAGE_FLAG_NX |
        PAGE_FLAG_DEVICE_MEMORY |
        PAGE_FLAG_NON_SHARABLE |
        PAGE_FLAG_ACCESS
    ) < 0) {
        return -1;
    }

    // NOTE: PCIe ECAM/MMCONFIG is within this range!!!!!!
    // TODO: This should be mapped by the pcie device driver instead of mmap directly.
    // VA 0x4010000000 - 0x401fffffff  -> PA 0x4010000000  RW EL1 NX DEVICE-nGnRnE NON-SH AF 
    if (mmap_map_range_l2_block(
        0x4010000000,
        0x401fffffff,
        0x4010000000,
        PAGE_FLAG_EL1_RW |
        PAGE_FLAG_NX |
        PAGE_FLAG_DEVICE_MEMORY |
        PAGE_FLAG_NON_SHARABLE |
        PAGE_FLAG_ACCESS
    ) < 0) {
        return -1;
    }

    // NOTE: xHCI is within this range!!!!!!
    // TODO: This should be mapped by the device driver instead of mmap directly.
    // VA 0x8000000000 - 0xffffffffff  -> PA 0x8000000000  RW EL1 NX DEVICE-nGnRnE NON-SH AF 
    if (mmap_map_range_l1_block(
        0x8000000000,
        0xffffffffff,
        0x8000000000,
        PAGE_FLAG_EL1_RW |
        PAGE_FLAG_NX |
        PAGE_FLAG_DEVICE_MEMORY |
        PAGE_FLAG_NON_SHARABLE |
        PAGE_FLAG_ACCESS
    ) < 0) {
        return -1;
    }

    // TODO: Build this range from the memory map.
    // VA 0x40000000 - 0x7fffffff -> PA 0x40000000  RW EL1 X NORMAL-WB INNER-SH AF 
    if (mmap_map_range(
        0x40000000,
        0x7fffffff,
        0x40000000,
        PAGE_FLAG_EL1_RW |
        PAGE_FLAG_NORMAL_MEMORY |
        PAGE_FLAG_INNER_SHARABLE |
        PAGE_FLAG_ACCESS
    ) < 0) {
        return -1;
    }

    return 0;
}

static int recursive_copy_table(uint64_t *src_table, uint64_t **out_table, int level) {
    uint64_t *dst_table = NULL;
    if (alloc_new_table(&dst_table) < 0) {
        return -1;
    }
    
    for (int i = 0; i < MMAP_GRANULE_SIZE / sizeof(uint64_t); i++) {
        uint64_t entry = src_table[i];

        // No valid entry, so nothing to do.
        if ((entry & TBL_ENTRY_VALID) == 0) {
            continue;
        }

        if ((entry & TBL_ENTRY_TABLE) != 0 && level < 3) {
            // Case 1: Entry is valid and a table entry (links to a lower level table).
            uint64_t *next_table = (uint64_t *)(entry & TBL_ENTRY_ADDR_MASK);
            uint64_t *new_table = NULL;
            if (recursive_copy_table(next_table, &new_table, level + 1) < 0) {
                return -1;
            }

            // Copy all page flags from the original entry, except for the address.
            dst_table[i] = (
                (entry & ~TBL_ENTRY_ADDR_MASK) |
                ((uint64_t)new_table & TBL_ENTRY_ADDR_MASK)
            );
        } else {
            // Maps to a block/page of memory, we want to copy the entry directly.
            dst_table[i] = entry;
        }
    }

    *out_table = dst_table;
    return 0;
}

int mmap_build_ttbr1_table() {
    // Copy the existing TTBR1 table to the kernel's translation table.
    // This table was created by our OS loader, so we can trust it to be valid.
    uint64_t ttbr1 = read_ttbr1();
    uint64_t *root = (uint64_t *)(ttbr1 & TBL_ENTRY_ADDR_MASK);
    return recursive_copy_table(root, &g_ttbr1_table, 0);
}

static int mmap_apply_ttbr0_table() {
    uint64_t ttbr0 = (uint64_t)g_ttbr0_table & TBL_ENTRY_ADDR_MASK;
    uint64_t tcr = read_tcr();

    // Clear the EPD0 flag to enable TTBR0 translations.
    tcr &= TCR_TTBR0_ENABLE_FLAG;

    // Set the TG0 bits to 0x0 to indicate 4kb granule size.
    tcr &= TCR_TTBR0_CLEAR_GRANULE_FLAG;
    tcr |= TCR_TTBR0_4KB_GRANULE_FLAG;
    
    // Set the T0SZ bits to 16 to indicate a 48 bit address space (64 - 16 = 48).
    tcr &= TCR_TTBR0_CLEAR_T1SZ_FLAG;
    tcr |= TCR_TTBR0_SET_T1SZ_16_FLAG;

    // Make sure all table writes hit memory before the CPU can walk them.
    __asm__ volatile("dsb ishst" ::: "memory");
    __asm__ volatile("isb");
    
    // Program the translation controls first.
    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr) : "memory");
    __asm__ volatile("isb");
    
    // Install the new TTBR0 root.
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"(ttbr0) : "memory");
    __asm__ volatile("isb");
    
    return 0;
}

static int mmap_apply_ttbr1_table() {
    uint64_t ttbr1 = (uint64_t)g_ttbr1_table & TBL_ENTRY_ADDR_MASK;
    uint64_t tcr = read_tcr();

    // Clear the EPD1 bit to enable TTBR1 translations.
    tcr &= TCR_TTBR1_ENABLE_FLAG;

    // Set the TG1 bits to 0x2 to indicate 4kb granule size.
    tcr &= TCR_TTBR1_CLEAR_GRANULE_FLAG;
    tcr |= TCR_TTBR1_4KB_GRANULE_FLAG;

    // Set the T1SZ bits to 16 to indicate a 48 bit address space (64 - 16 = 48).
    tcr &= TCR_TTBR1_CLEAR_T1SZ_FLAG;
    tcr |= TCR_TTBR1_SET_T1SZ_16_FLAG;

    // Make sure all table writes hit memory before the CPU can walk them.
    __asm__ volatile("dsb ishst" ::: "memory");
    __asm__ volatile("isb");
  
    // Program the translation controls first.
    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr) : "memory");
    __asm__ volatile("isb");
  
    // Install the new TTBR1 root.
    __asm__ volatile("msr ttbr1_el1, %0" :: "r"(ttbr1) : "memory");
    __asm__ volatile("isb");

    return 0;
}

int mmap_apply_mappings(void) {
    // Apply the TTBR0 table.
    if (mmap_apply_ttbr0_table() < 0) {
        console_write("Failed to apply TTBR0 table\r\n");
        return -1;
    }

    // Apply the TTBR1 table.
    if (mmap_apply_ttbr1_table() < 0) {
        console_write("Failed to apply TTBR1 table\r\n");
        return -1;
    }

    // Invalidate stale EL1 stage-1 translations.
    __asm__ volatile("tlbi vmalle1");
    __asm__ volatile("dsb ish");
    __asm__ volatile("isb");

    return 0;
}

int mmap_init(
    efi_memory_descriptor_t *efi_memory_map,
    uint64_t efi_memory_map_size,
    uint64_t efi_memory_map_descriptor_size
) {
    // Reserve memory for mmap to use during operation.
    // This system is initialized before the page allocator, as we need to map memory before we can track / allocate pages.
    // If we try to initialize the page allocator before this, we get data abort exceptions when trying to read/write free memory.
    if (mmap_reserve_memory(efi_memory_map, efi_memory_map_size, efi_memory_map_descriptor_size) < 0) {
        console_write("Failed to reserve memory for mmap\r\n");
        return -1;
    }

    if (mmap_build_memory_map(efi_memory_map, efi_memory_map_size, efi_memory_map_descriptor_size) < 0) {
        console_write("Failed to build memory map\r\n");
        return -1;
    }

    // Build the TTBR0 table.
    if (mmap_build_ttbr0_table() < 0) {
        console_write("Failed to build TTBR0 table\r\n");
        return -1;
    }

    // Build the TTBR1 table.
    if (mmap_build_ttbr1_table() < 0) {
        console_write("Failed to build TTBR1 table\r\n");
        return -1;
    }

    // Apply the mappings to the CPUs translation tables.
    if (mmap_apply_mappings() < 0) {
        console_write("Failed to apply mappings to the CPUs translation tables\r\n");
        return -1;
    }

    efi_memory_map_print_details(efi_memory_map, efi_memory_map_size, efi_memory_map_descriptor_size);
    print_current_mair_attributes();
    print_ttbr0_table();

    uint64_t tcr = read_tcr();
    console_write("TCR: ");
    console_write_hex(tcr);
    console_write("\r\n");

    return 0;
}

// Maps 1GB block of virtual addresses to a 1GB block of physical addresses.
int mmap_map_l1_block(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t page_flags
) {
    if (virtual_address % TBL_L1_BLOCK_SIZE != 0) {
        console_write("Virtual address is not aligned to 1GB\r\n");
        return -1;
    }

    if (physical_address % TBL_L1_BLOCK_SIZE != 0) {
        console_write("Physical address is not aligned to 1GB\r\n");
        return -1;
    }

    uint64_t l0_index = (virtual_address >> 39) & 0x1FF;
    uint64_t l1_index = (virtual_address >> 30) & 0x1FF;

    uint64_t **ttbr_table_ptr = get_ttbr_table_ptr(virtual_address);
    if (*ttbr_table_ptr == NULL) {
        // Allocate the table.
        if (alloc_new_table(ttbr_table_ptr) < 0) {
            console_write("Failed to allocate new TTBR table for virtual address ");
            console_write_hex(virtual_address);
            console_write("\r\n");
            return -1;
        }
    }

    uint64_t *ttbr_table = *ttbr_table_ptr;
    uint64_t l0_entry = ttbr_table[l0_index];
    if ((l0_entry & TBL_ENTRY_VALID) == 0) {
        uint64_t *new_table = NULL;
        if (alloc_new_table(&new_table) < 0) {
            return -1;
        }

        ttbr_table[l0_index] = (
            TBL_ENTRY_VALID |
            TBL_ENTRY_TABLE |
            ((uint64_t)new_table & TBL_ENTRY_ADDR_MASK)
        );
        l0_entry = ttbr_table[l0_index];
    }

    if ((l0_entry & TBL_ENTRY_TABLE) == 0) {
        // Something is already mapped here, so we can't map.
        return -1;
    }

    uint64_t *l1_page_table = (uint64_t *)(l0_entry & TBL_ENTRY_ADDR_MASK);
    uint64_t l1_entry = l1_page_table[l1_index];
    if ((l1_entry & TBL_ENTRY_VALID) == 0) {
        l1_page_table[l1_index] = (
            TBL_ENTRY_VALID | 
            TBL_ENTRY_BLOCK |
            PAGE_FLAG_ACCESS |
            page_flags |
            (physical_address & TBL_ENTRY_L1_BLOCK_ADDR_MASK)
        );
        return 0;
    } else {
        // Somrthing is already mapped here, so we can't map.
        return -1;
    }
}

int mmap_map_range_l1_block(
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
) {
    if (physical_start_address % TBL_L1_BLOCK_SIZE != 0) {
        console_write("Physical address is not aligned to 1GB\r\n");
        return -1;
    }

    if (virtual_start_address % TBL_L1_BLOCK_SIZE != 0) {
        console_write("Virtual address is not aligned to 1GB\r\n");
        return -1;
    }

    for (
        uint64_t virtual_address = virtual_start_address;
        virtual_address < virtual_end_address;
        virtual_address += TBL_L1_BLOCK_SIZE,
        physical_start_address += TBL_L1_BLOCK_SIZE
    ) {
        if (mmap_map_l1_block(virtual_address, physical_start_address, page_flags) < 0) {
            return -1;
        }
    }

    return 0;
}

int mmap_map_l2_block(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t page_flags
) {
    if (virtual_address % TBL_L2_BLOCK_SIZE != 0) {
        console_write("Virtual address is not aligned to 2MB\r\n");
        return -1;
    }

    if (physical_address % TBL_L2_BLOCK_SIZE != 0) {
        console_write("Physical address is not aligned to 2MB\r\n");
        return -1;
    }

    uint64_t l0_index = (virtual_address >> 39) & 0x1FF;
    uint64_t l1_index = (virtual_address >> 30) & 0x1FF;
    uint64_t l2_index = (virtual_address >> 21) & 0x1FF;

    uint64_t **ttbr_table_ptr = get_ttbr_table_ptr(virtual_address);
    if (*ttbr_table_ptr == NULL) {
        // Allocate the table.
        if (alloc_new_table(ttbr_table_ptr) < 0) {
            return -1;
        }
    }

    uint64_t *ttbr_table = *ttbr_table_ptr;
    uint64_t l0_entry = ttbr_table[l0_index];
    if ((l0_entry & TBL_ENTRY_VALID) == 0) {
        uint64_t *new_table = NULL;
        if (alloc_new_table(&new_table) < 0) {
            return -1;
        }

        ttbr_table[l0_index] = (
            TBL_ENTRY_VALID |
            TBL_ENTRY_TABLE |
            ((uint64_t)new_table & TBL_ENTRY_ADDR_MASK)
        );
        l0_entry = ttbr_table[l0_index];
    }

    if ((l0_entry & TBL_ENTRY_TABLE) == 0) {
        // Something is already mapped here, so we can't map.
        return -1;
    }

    uint64_t *l1_page_table = (uint64_t *)(l0_entry & TBL_ENTRY_ADDR_MASK);
    uint64_t l1_entry = l1_page_table[l1_index];
    if ((l1_entry & TBL_ENTRY_VALID) == 0) {
        uint64_t *new_table = NULL;
        if (alloc_new_table(&new_table) < 0) {
            return -1;
        }
        l1_page_table[l1_index] = (
            TBL_ENTRY_VALID |
            TBL_ENTRY_TABLE |
            ((uint64_t)new_table & TBL_ENTRY_ADDR_MASK)
        );
        l1_entry = l1_page_table[l1_index];
    }

    if ((l1_entry & TBL_ENTRY_TABLE) == 0) {
        // Something is already mapped here, so we can't map.
        return -1;
    }

    uint64_t *l2_page_table = (uint64_t *)(l1_entry & TBL_ENTRY_ADDR_MASK);
    uint64_t l2_entry = l2_page_table[l2_index];
    if ((l2_entry & TBL_ENTRY_VALID) == 0) {
        l2_page_table[l2_index] = (
            TBL_ENTRY_VALID |
            TBL_ENTRY_BLOCK |
            PAGE_FLAG_ACCESS |
            page_flags |
            (physical_address & TBL_ENTRY_L2_BLOCK_ADDR_MASK)
        );
        return 0;
    } else {
        // Something is already mapped here, so we can't map.
        return -1;
    }
}

int mmap_map_range_l2_block(
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
) {
    if (physical_start_address % TBL_L2_BLOCK_SIZE != 0) {
        console_write("Physical address is not aligned to 2MB\r\n");
        return -1;
    }

    if (virtual_start_address % TBL_L2_BLOCK_SIZE != 0) {
        console_write("Virtual address is not aligned to 2MB\r\n");
        return -1;
    }

    for (
        uint64_t virtual_address = virtual_start_address;
        virtual_address < virtual_end_address;
        virtual_address += TBL_L2_BLOCK_SIZE,
        physical_start_address += TBL_L2_BLOCK_SIZE
    ) {
        if (mmap_map_l2_block(virtual_address, physical_start_address, page_flags) < 0) {
            return -1;
        }
    }

    return 0;
}

int mmap_map_range(
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
) {
    for (
        uint64_t virtual_address = virtual_start_address;
        virtual_address < virtual_end_address;
        virtual_address += MMAP_GRANULE_SIZE,
        physical_start_address += MMAP_GRANULE_SIZE
    ) {
        if (mmap_map_page(physical_start_address, virtual_address, page_flags) < 0) {
            return -1;
        }
    }

    return 0;
}

int mmap_map_page(
    uint64_t physical_address,
    uint64_t virtual_address,
    uint64_t page_flags
) {
    // Extract level indices from the virtual address (48-bit address space).
    // 47..39 = L0 index
    // 38..30 = L1 index
    // 29..21 = L2 index
    // 20..12 = L3 index
    // 11..0 = Offset
    uint64_t l0_index = (virtual_address >> 39) & 0x1FF;
    uint64_t l1_index = (virtual_address >> 30) & 0x1FF;
    uint64_t l2_index = (virtual_address >> 21) & 0x1FF;
    uint64_t l3_index = (virtual_address >> 12) & 0x1FF;

    uint64_t **ttbr_table_ptr = get_ttbr_table_ptr(virtual_address);
    if (*ttbr_table_ptr == NULL) {
        // Allocate the table.
        if (alloc_new_table(ttbr_table_ptr) < 0) {
            console_write("Failed to allocate new TTBR table for virtual address ");
            console_write_hex(virtual_address);
            console_write("\r\n");
            return -1;
        }
    }

    uint64_t *ttbr_table = *ttbr_table_ptr;
    uint64_t l0_entry = ttbr_table[l0_index];
    if ((l0_entry & TBL_ENTRY_VALID) == 0) {
        uint64_t *new_table = NULL;
        if (alloc_new_table(&new_table) < 0) {
            return -1;
        }

        ttbr_table[l0_index] = (
            TBL_ENTRY_VALID |
            TBL_ENTRY_TABLE |
            ((uint64_t)new_table & TBL_ENTRY_ADDR_MASK)
        );
        l0_entry = ttbr_table[l0_index];
    }
    
    uint64_t *l0_page_table = (uint64_t *)(l0_entry & TBL_ENTRY_ADDR_MASK);
    uint64_t l1_entry = l0_page_table[l1_index];
    if ((l1_entry & TBL_ENTRY_VALID) == 0) {
        uint64_t *new_table = NULL;
        if (alloc_new_table(&new_table) < 0) {
            return -1;
        }

        l0_page_table[l1_index] = (
            TBL_ENTRY_VALID |
            TBL_ENTRY_TABLE |
            ((uint64_t)new_table & TBL_ENTRY_ADDR_MASK)
        );
        l1_entry = l0_page_table[l1_index];
    }

    uint64_t *l1_page_table = (uint64_t *)(l1_entry & TBL_ENTRY_ADDR_MASK);
    uint64_t l2_entry = l1_page_table[l2_index];
    if ((l2_entry & TBL_ENTRY_VALID) == 0) {
        uint64_t *new_table = NULL;
        if (alloc_new_table(&new_table) < 0) {
            return -1;
        }

        l1_page_table[l2_index] = (
            TBL_ENTRY_VALID |
            TBL_ENTRY_TABLE |
            ((uint64_t)new_table & TBL_ENTRY_ADDR_MASK)
        );
        l2_entry = l1_page_table[l2_index];
    }

    uint64_t *l2_page_table = (uint64_t *)(l2_entry & TBL_ENTRY_ADDR_MASK);
    l2_page_table[l3_index] = (
        TBL_ENTRY_VALID |
        TBL_ENTRY_TABLE |
        PAGE_FLAG_ACCESS |
        page_flags |
        (physical_address & TBL_ENTRY_ADDR_MASK)
    );

    return 0;
}

int mmap_virtual_to_physical(
    uint64_t virtual_address,
    uint64_t *physical_address
) {
    uint64_t **ttbr_table_ptr = get_ttbr_table_ptr(virtual_address);
    if (*ttbr_table_ptr == NULL) {
        return -1;
    }

    uint64_t *ttbr_table = *ttbr_table_ptr;
    uint64_t l0_index = (virtual_address >> 39) & 0x1FF;
    uint64_t l1_index = (virtual_address >> 30) & 0x1FF;
    uint64_t l2_index = (virtual_address >> 21) & 0x1FF;
    uint64_t l3_index = (virtual_address >> 12) & 0x1FF;

    uint64_t l0_entry = ttbr_table[l0_index];
    if ((l0_entry & TBL_ENTRY_VALID) == 0) {
        return -1;
    }

    uint64_t *l0_page_table = (uint64_t *)(l0_entry & TBL_ENTRY_ADDR_MASK);
    uint64_t l1_entry = l0_page_table[l1_index];
    if ((l1_entry & TBL_ENTRY_VALID) == 0) {
        return -1;
    }
    
    // Block entry, return the physical address.
    if ((l1_entry & TBL_ENTRY_TABLE) == 0) {
        *physical_address = (l1_entry & TBL_ENTRY_L1_BLOCK_ADDR_MASK) | (virtual_address & (TBL_L1_BLOCK_SIZE - 1));
        return 0;
    }

    uint64_t *l1_page_table = (uint64_t *)(l1_entry & TBL_ENTRY_ADDR_MASK);
    uint64_t l2_entry = l1_page_table[l2_index];
    if ((l2_entry & TBL_ENTRY_VALID) == 0) {
        return -1;
    }

    // Block entry, return the physical address.
    if ((l2_entry & TBL_ENTRY_TABLE) == 0) {
        *physical_address = (l2_entry & TBL_ENTRY_L2_BLOCK_ADDR_MASK) | (virtual_address & (TBL_L2_BLOCK_SIZE - 1));
        return 0;
    }

    uint64_t *l2_page_table = (uint64_t *)(l2_entry & TBL_ENTRY_ADDR_MASK);
    uint64_t l3_entry = l2_page_table[l3_index];
    if ((l3_entry & TBL_ENTRY_VALID) == 0) {
        return -1;
    }

    // Page entry, return the physical address.
    *physical_address = (l3_entry & TBL_ENTRY_ADDR_MASK) | (virtual_address & (MMAP_GRANULE_SIZE - 1));
    return 0;
}
