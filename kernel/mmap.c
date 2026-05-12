#include "mmap.h"
#include "bump_allocator.h"
#include "format.h"
#include "console.h"
#include "math.h"
#include "memory.h"
#include "assert.h"
#include "vmap.h"

#include <stddef.h>

#define MMAP_VIRTUAL_BASE_ADDRESS 0xFFFF500000000000ULL
#define MMAP_PHYSICAL_REGION_SIZE (1ULL << 34) // 16GB
#define MMAP_PAGE_FLAGS PAGE_FLAG_EL1_RW | PAGE_FLAG_NX | PAGE_FLAG_NORMAL_MEMORY | PAGE_FLAG_INNER_SHARABLE | PAGE_FLAG_ACCESS

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

static uint64_t* g_ttbr0_table = NULL;
static uint64_t* g_ttbr1_table = NULL;
static uint8_t g_mmap_initialized = 0;

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

static int alloc_new_table(uint64_t **new_table) {
    if (bump_allocator_invalid(&g_page_table_allocator)) {
        console_write("Unable to allocate new TTBR0 table, no reserved page table memory available\r\n");
        return -1;
    }

    uint64_t new_page_table_physical_addr = 0;
    if (bump_allocator_allocate(&g_page_table_allocator, MMAP_GRANULE_SIZE, &new_page_table_physical_addr) < 0) {
        console_write("Unable to allocate new TTBR0 table, no pages left in the reserved page table memory\r\n");
        return -1;
    }

    // If the mmap system is already initialized, we need to convert the physical address to a virtual address.
    if (g_mmap_initialized) {
        uint64_t new_page_table_virtual_addr = 0;
        assert(mmap_physical_to_virtual(new_page_table_physical_addr, &new_page_table_virtual_addr) == 0);
        memory_set((void *)new_page_table_virtual_addr, 0, MMAP_GRANULE_SIZE);
    } else {
        memory_set((void *)new_page_table_physical_addr, 0, MMAP_GRANULE_SIZE);
    }

    // Set the new table pointer.
    *new_table = (uint64_t *)new_page_table_physical_addr;
    return 0;
}

static int mmap_reserve_memory(
    efi_memory_descriptor_t *efi_memory_map,
    uint64_t efi_memory_map_size,
    uint64_t efi_memory_map_descriptor_size
) {
    assert(g_mmap_initialized == 0);

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
    assert(g_mmap_initialized == 0);

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
    assert(g_mmap_initialized == 0);

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

    if (g_mmap_initialized) {
        // Map the physical memory map to virtual address space.
        assert(mmap_physical_to_virtual(g_kernel_memory_map_allocator.memory_start, (uint64_t *)out_memory_map) == 0);
    } else {
        // Still return just the physical address, as the mmap system is not initialized yet.
        *out_memory_map = (mmap_memory_descriptor_t *)g_kernel_memory_map_allocator.memory_start;
    }

    *out_memory_map_count = g_kernel_memory_map_count;
    return 0;
}

int mmap_build_ttbr0_table() {
    assert(g_mmap_initialized == 0);

    // Allocate the root table (first 512 entries).
    if (alloc_new_table(&g_ttbr0_table) < 0) {
        return -1;
    }

    return 0;
}

static int recursive_copy_table(uint64_t *src_table, uint64_t **out_table, int level) {
    assert(g_mmap_initialized == 0);

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
    assert(g_mmap_initialized == 0);

    // Copy the existing TTBR1 table to the kernel's translation table.
    // This table was created by our OS loader, so we can trust it to be valid.
    uint64_t ttbr1 = read_ttbr1();
    uint64_t *root = (uint64_t *)(ttbr1 & TBL_ENTRY_ADDR_MASK);
    int result = recursive_copy_table(root, &g_ttbr1_table, 0);
    if (result < 0) {
        console_write("Failed to copy TTBR1 table\r\n");
        return -1;
    }

    // Map physical space to kernel's virtual address space.
    if (mmap_map_range_l1_block(
        MMAP_VIRTUAL_BASE_ADDRESS,
        MMAP_VIRTUAL_BASE_ADDRESS + MMAP_PHYSICAL_REGION_SIZE,
        0x0ULL,
        PAGE_FLAG_EL1_RW |
        PAGE_FLAG_NX |
        PAGE_FLAG_NORMAL_MEMORY |
        PAGE_FLAG_INNER_SHARABLE |
        PAGE_FLAG_ACCESS
    ) < 0) {
        console_write("Failed to map physical space to kernel's virtual address space\r\n");
        return -1;
    }

    return 0;
}

static uint64_t alloc_vmap_page(void) {
    uint64_t *page;
    if (alloc_new_table(&page) < 0) {
        return 0;
    }
    return (uint64_t)page;
}

static uint64_t vmap_physical_to_virtual(uint64_t physical_address) {
    uint64_t virtual_address;
    if (mmap_physical_to_virtual(physical_address, &virtual_address) < 0) {
        return 0;
    }
    return virtual_address;
}

static int build_vmap_table(uint64_t **ttbr_table_ptr, vmap_t *vmap) {
    uint64_t *ttbr_table = *ttbr_table_ptr;
    if (vmap_init(vmap, alloc_vmap_page, vmap_physical_to_virtual) < 0) {
        return -1;
    }
    vmap->root_page_table = (uint64_t)ttbr_table;
    return 0;
}

int mmap_build_vmap(vmap_t *vmap, uint64_t virtual_address) {
    uint64_t **ttbr_table_ptr = get_ttbr_table_ptr(virtual_address);
    if (*ttbr_table_ptr == NULL) {
        return -1;
    }
    return build_vmap_table(ttbr_table_ptr, vmap);
}

int mmap_apply_mappings(void) {
    vmap_t ttbr0_vmap;
    if (vmap_init(&ttbr0_vmap, alloc_vmap_page, vmap_physical_to_virtual) < 0) {
        return -1;
    }
    ttbr0_vmap.root_page_table = (uint64_t)g_ttbr0_table;
    if (vmap_apply_table(&ttbr0_vmap, VMAP_DESTINATION_USER) < 0) {
        return -1;
    }

    vmap_t ttbr1_vmap;
    if (vmap_init(&ttbr1_vmap, alloc_vmap_page, vmap_physical_to_virtual) < 0) {
        return -1;
    }
    ttbr1_vmap.root_page_table = (uint64_t)g_ttbr1_table;
    if (vmap_apply_table(&ttbr1_vmap, VMAP_DESTINATION_KERNEL) < 0) {
        return -1;
    }

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

    g_mmap_initialized = 1;

    return 0;
}

void mmap_debug_print(
    efi_memory_descriptor_t *efi_memory_map,
    uint64_t efi_memory_map_size,
    uint64_t efi_memory_map_descriptor_size
) {
    efi_memory_map_print_details(efi_memory_map, efi_memory_map_size, efi_memory_map_descriptor_size);
    print_current_mair_attributes();
}

int mmap_map_l1_block(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t page_flags
) {
    uint64_t **ttbr_table_ptr = get_ttbr_table_ptr(virtual_address);

    vmap_t vmap;
    if (build_vmap_table(ttbr_table_ptr, &vmap) < 0) {
        return -1;
    }

    if (vmap_map_l1_block(&vmap, virtual_address, physical_address, page_flags) < 0) {
        return -1;
    }

    if (*ttbr_table_ptr == NULL) {
        *ttbr_table_ptr = (uint64_t*)vmap.root_page_table;
    }

    return 0;
}

int mmap_map_range_l1_block(
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
) {
    uint64_t **ttbr_table_ptr = get_ttbr_table_ptr(virtual_start_address);

    vmap_t vmap;
    if (build_vmap_table(ttbr_table_ptr, &vmap) < 0) {
        return -1;
    }

    if (vmap_map_range_l1_block(
        &vmap,
        virtual_start_address,
        virtual_end_address,
        physical_start_address,
        page_flags
    ) < 0) {
        return -1;
    }

    if (*ttbr_table_ptr == NULL) {
        *ttbr_table_ptr = (uint64_t*)vmap.root_page_table;
    }

    return 0;
}

int mmap_map_l2_block(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t page_flags
) {
    uint64_t **ttbr_table_ptr = get_ttbr_table_ptr(virtual_address);

    vmap_t vmap;
    if (build_vmap_table(ttbr_table_ptr, &vmap) < 0) {
        return -1;
    }

    if (vmap_map_l2_block(
        &vmap,
        virtual_address,
        physical_address,
        page_flags
    ) < 0) {
        return -1;
    }

    if (*ttbr_table_ptr == NULL) {
        *ttbr_table_ptr = (uint64_t*)vmap.root_page_table;
    }

    return 0;
}

int mmap_map_range_l2_block(
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
) {
    uint64_t **ttbr_table_ptr = get_ttbr_table_ptr(virtual_start_address);

    vmap_t vmap;
    if (build_vmap_table(ttbr_table_ptr, &vmap) < 0) {
        return -1;
    }

    if (vmap_map_range_l2_block(
        &vmap,
        virtual_start_address,
        virtual_end_address,
        physical_start_address,
        page_flags
    ) < 0) {
        return -1;
    }

    if (*ttbr_table_ptr == NULL) {
        *ttbr_table_ptr = (uint64_t*)vmap.root_page_table;
    }

    return 0;
}

int mmap_map_range(
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
) {
    uint64_t **ttbr_table_ptr = get_ttbr_table_ptr(virtual_start_address);

    vmap_t vmap;
    if (build_vmap_table(ttbr_table_ptr, &vmap) < 0) {
        return -1;
    }

    if (vmap_map_range(
        &vmap,
        virtual_start_address,
        virtual_end_address,
        physical_start_address,
        page_flags
    ) < 0) {
        return -1;
    }

    if (*ttbr_table_ptr == NULL) {
        *ttbr_table_ptr = (uint64_t*)vmap.root_page_table;
    }

    return 0;
}

int mmap_map_page(
    uint64_t physical_address,
    uint64_t virtual_address,
    uint64_t page_flags
) {
    uint64_t **ttbr_table_ptr = get_ttbr_table_ptr(virtual_address);

    vmap_t vmap;
    if (build_vmap_table(ttbr_table_ptr, &vmap) < 0) {
        return -1;
    }
    
    if (vmap_map_page(
        &vmap,
        physical_address,
        virtual_address,
        page_flags
    ) < 0) {
        return -1;
    }
    return 0;

    if (*ttbr_table_ptr == NULL) {
        *ttbr_table_ptr = (uint64_t*)vmap.root_page_table;
    }

    return 0;
}

int mmap_physical_to_virtual(
    uint64_t physical_address,
    uint64_t *virtual_address
) {
    // Identity map if mmap is not initialized.
    if (!g_mmap_initialized) {
        *virtual_address = physical_address;
        return 0;
    }

    if (physical_address < 0x0ULL || physical_address >= MMAP_PHYSICAL_REGION_SIZE) {
        console_write("Physical address is out of range\r\n");
        return -1;
    }

    *virtual_address = MMAP_VIRTUAL_BASE_ADDRESS + physical_address;
    return 0;
}

int mmap_virtual_to_physical(
    uint64_t virtual_address,
    uint64_t *physical_address
) {
    vmap_t vmap;
    if (build_vmap_table(get_ttbr_table_ptr(virtual_address), &vmap) < 0) {
        return -1;
    }
    return vmap_virtual_to_physical(&vmap, virtual_address, physical_address);
}
