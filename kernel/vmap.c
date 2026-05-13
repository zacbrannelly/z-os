#include "vmap.h"
#include "mmap.h"
#include "console.h"
#include "memory.h"

#include <stddef.h>

#define L0_INDEX(virtual_address) ((virtual_address >> 39) & 0x1FF)
#define L1_INDEX(virtual_address) ((virtual_address >> 30) & 0x1FF)
#define L2_INDEX(virtual_address) ((virtual_address >> 21) & 0x1FF)
#define L3_INDEX(virtual_address) ((virtual_address >> 12) & 0x1FF)

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

int vmap_init(
    vmap_t *vmap,
    vmap_alloc_page_fn_t alloc_page,
    vmap_physical_to_virtual_fn_t physical_to_virtual
) {
    memory_set(vmap, 0, sizeof(vmap_t));
    vmap->alloc_page = alloc_page;
    vmap->physical_to_virtual = physical_to_virtual;
    return 0;
}

// Maps 1GB block of virtual addresses to a 1GB block of physical addresses.
int vmap_map_l1_block(
    vmap_t *vmap,
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

    uint64_t *ttbr_table = (uint64_t *)vmap->root_page_table;
    if (ttbr_table == NULL) {
        ttbr_table = (uint64_t *)vmap->alloc_page();
        if (ttbr_table == NULL) {
            return -1;
        }
        vmap->root_page_table = (uint64_t)ttbr_table;
    }

    ttbr_table = (uint64_t *)vmap->physical_to_virtual(vmap->root_page_table);

    uint64_t l0_index = L0_INDEX(virtual_address);
    uint64_t l0_entry = ttbr_table[l0_index];
    if ((l0_entry & TBL_ENTRY_VALID) == 0) {
        uint64_t *new_table = (uint64_t *)vmap->alloc_page();
        if (new_table == NULL) {
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

    uint64_t *l1_page_table = (uint64_t *)vmap->physical_to_virtual((uint64_t)(l0_entry & TBL_ENTRY_ADDR_MASK));
    uint64_t l1_index = L1_INDEX(virtual_address);
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

int vmap_map_range_l1_block(
    vmap_t *vmap,
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
) {
    // TODO: This assumes the physical pages are contiguous. This is not always the case.
    for (
        uint64_t virtual_address = virtual_start_address;
        virtual_address < virtual_end_address;
        virtual_address += TBL_L1_BLOCK_SIZE,
        physical_start_address += TBL_L1_BLOCK_SIZE
    ) {
        if (vmap_map_l1_block(vmap, virtual_address, physical_start_address, page_flags) < 0) {
            return -1;
        }
    }

    return 0;
}

int vmap_map_l2_block(
    vmap_t *vmap,
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

    uint64_t *ttbr_table = (uint64_t *)vmap->root_page_table;

    if (ttbr_table == NULL) {
        ttbr_table = (uint64_t *)vmap->alloc_page();
        if (ttbr_table == NULL) {
            return -1;
        }
        vmap->root_page_table = (uint64_t)ttbr_table;
    }

    ttbr_table = (uint64_t *)vmap->physical_to_virtual(vmap->root_page_table);

    uint64_t l0_index = L0_INDEX(virtual_address);
    uint64_t l0_entry = ttbr_table[l0_index];
    if ((l0_entry & TBL_ENTRY_VALID) == 0) {
        uint64_t *new_table = (uint64_t *)vmap->alloc_page();
        if (new_table == NULL) {
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

    uint64_t *l1_page_table = (uint64_t *)vmap->physical_to_virtual((uint64_t)(l0_entry & TBL_ENTRY_ADDR_MASK));
    uint64_t l1_index = L1_INDEX(virtual_address);
    uint64_t l1_entry = l1_page_table[l1_index];
    if ((l1_entry & TBL_ENTRY_VALID) == 0) {
        uint64_t *new_table = (uint64_t *)vmap->alloc_page();
        if (new_table == NULL) {
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

    uint64_t *l2_page_table = (uint64_t *)vmap->physical_to_virtual((uint64_t)(l1_entry & TBL_ENTRY_ADDR_MASK));
    uint64_t l2_index = L2_INDEX(virtual_address);
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

int vmap_map_range_l2_block(
    vmap_t *vmap,
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
) {
    // TODO: This assumes the physical pages are contiguous. This is not always the case.
    for (
        uint64_t virtual_address = virtual_start_address;
        virtual_address < virtual_end_address;
        virtual_address += TBL_L2_BLOCK_SIZE,
        physical_start_address += TBL_L2_BLOCK_SIZE
    ) {
        if (vmap_map_l2_block(vmap, virtual_address, physical_start_address, page_flags) < 0) {
            return -1;
        }
    }

    return 0;
}

int vmap_map_range(
    vmap_t *vmap,
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
) {
    // TODO: This assumes the physical pages are contiguous. This is not always the case.
    for (
        uint64_t virtual_address = virtual_start_address;
        virtual_address < virtual_end_address;
        virtual_address += MMAP_GRANULE_SIZE,
        physical_start_address += MMAP_GRANULE_SIZE
    ) {
        if (vmap_map_page(vmap, physical_start_address, virtual_address, page_flags) < 0) {
            return -1;
        }
    }

    return 0;
}

int vmap_map_page(
    vmap_t *vmap,
    uint64_t physical_address,
    uint64_t virtual_address,
    uint64_t page_flags
) {
    uint64_t *ttbr_table = (uint64_t *)vmap->root_page_table;
    if (ttbr_table == NULL) {
        ttbr_table = (uint64_t *)vmap->alloc_page();
        if (ttbr_table == NULL) {
            return -1;
        }
        vmap->root_page_table = (uint64_t)ttbr_table;
    }

    ttbr_table = (uint64_t *)vmap->physical_to_virtual(vmap->root_page_table);

    uint64_t l0_index = L0_INDEX(virtual_address);
    uint64_t l0_entry = ttbr_table[l0_index];
    if ((l0_entry & TBL_ENTRY_VALID) == 0) {
        uint64_t *new_table = (uint64_t *)vmap->alloc_page();
        if (new_table == NULL) {
            return -1;
        }

        ttbr_table[l0_index] = (
            TBL_ENTRY_VALID |
            TBL_ENTRY_TABLE |
            ((uint64_t)new_table & TBL_ENTRY_ADDR_MASK)
        );
        l0_entry = ttbr_table[l0_index];
    }
    
    uint64_t *l0_page_table = (uint64_t *)vmap->physical_to_virtual((uint64_t)(l0_entry & TBL_ENTRY_ADDR_MASK));
    uint64_t l1_index = L1_INDEX(virtual_address);
    uint64_t l1_entry = l0_page_table[l1_index];
    if ((l1_entry & TBL_ENTRY_VALID) == 0) {
        uint64_t *new_table = (uint64_t *)vmap->alloc_page();
        if (new_table == NULL) {
            return -1;
        }

        l0_page_table[l1_index] = (
            TBL_ENTRY_VALID |
            TBL_ENTRY_TABLE |
            ((uint64_t)new_table & TBL_ENTRY_ADDR_MASK)
        );
        l1_entry = l0_page_table[l1_index];
    }

    uint64_t *l1_page_table = (uint64_t *)vmap->physical_to_virtual((uint64_t)(l1_entry & TBL_ENTRY_ADDR_MASK));
    uint64_t l2_index = L2_INDEX(virtual_address);
    uint64_t l2_entry = l1_page_table[l2_index];
    if ((l2_entry & TBL_ENTRY_VALID) == 0) {
        uint64_t *new_table = (uint64_t *)vmap->alloc_page();
        if (new_table == NULL) {
            return -1;
        }

        l1_page_table[l2_index] = (
            TBL_ENTRY_VALID |
            TBL_ENTRY_TABLE |
            ((uint64_t)new_table & TBL_ENTRY_ADDR_MASK)
        );
        l2_entry = l1_page_table[l2_index];
    }

    uint64_t *l2_page_table = (uint64_t *)vmap->physical_to_virtual((uint64_t)(l2_entry & TBL_ENTRY_ADDR_MASK));
    uint64_t l3_index = L3_INDEX(virtual_address);
    l2_page_table[l3_index] = (
        TBL_ENTRY_VALID |
        TBL_ENTRY_TABLE |
        PAGE_FLAG_ACCESS |
        page_flags |
        (physical_address & TBL_ENTRY_ADDR_MASK)
    );

    return 0;
}

static uint64_t read_tcr(void) {
    uint64_t tcr = 0;
    __asm__("mrs %0, tcr_el1" : "=r" (tcr));
    return tcr;
}

int vmap_apply_table(
    vmap_t *vmap,
    vmap_destination_t destination
) {
    uint64_t tcr = read_tcr();

    if (destination == VMAP_DESTINATION_USER) {
        // Clear the EPD0 flag to enable TTBR0 translations.
        tcr &= TCR_TTBR0_ENABLE_FLAG;

        // Set the TG0 bits to 0x0 to indicate 4kb granule size.
        tcr &= TCR_TTBR0_CLEAR_GRANULE_FLAG;
        tcr |= TCR_TTBR0_4KB_GRANULE_FLAG;
        
        // Set the T0SZ bits to 16 to indicate a 48 bit address space (64 - 16 = 48).
        tcr &= TCR_TTBR0_CLEAR_T1SZ_FLAG;
        tcr |= TCR_TTBR0_SET_T1SZ_16_FLAG;
    } else if (destination == VMAP_DESTINATION_KERNEL) {
        // Clear the EPD1 bit to enable TTBR1 translations.
        tcr &= TCR_TTBR1_ENABLE_FLAG;

        // Set the TG1 bits to 0x2 to indicate 4kb granule size.
        tcr &= TCR_TTBR1_CLEAR_GRANULE_FLAG;
        tcr |= TCR_TTBR1_4KB_GRANULE_FLAG;

        // Set the T1SZ bits to 16 to indicate a 48 bit address space (64 - 16 = 48).
        tcr &= TCR_TTBR1_CLEAR_T1SZ_FLAG;
        tcr |= TCR_TTBR1_SET_T1SZ_16_FLAG;
    } else {
        console_write("Invalid destination\r\n");
        return -1;
    }

    // Make sure all table writes hit memory before the CPU can walk them.
    __asm__ volatile("dsb ishst" ::: "memory");
    __asm__ volatile("isb");
    
    // Program the translation controls first.
    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr) : "memory");
    __asm__ volatile("isb");
    
    if (destination == VMAP_DESTINATION_USER) {
        // Install the new TTBR0 root.
        __asm__ volatile("msr ttbr0_el1, %0" :: "r"(vmap->root_page_table & ~0x1ULL) : "memory");
    } else {
        // Install the new TTBR1 root.
        __asm__ volatile("msr ttbr1_el1, %0" :: "r"(vmap->root_page_table & ~0x1ULL) : "memory");
    }
    __asm__ volatile("isb");

    // Invalidate stale EL1 stage-1 translations.
    __asm__ volatile("tlbi vmalle1");
    __asm__ volatile("dsb ish");
    __asm__ volatile("isb");

    return 0;
}

int vmap_virtual_to_physical(
    vmap_t *vmap,
    uint64_t virtual_address,
    uint64_t *physical_address
) {
    uint64_t *ttbr_table = (uint64_t *)vmap->root_page_table;
    if (ttbr_table == NULL) {
        return -1;
    }

    ttbr_table = (uint64_t *)vmap->physical_to_virtual(vmap->root_page_table);

    uint64_t l0_index = L0_INDEX(virtual_address);
    uint64_t l0_entry = ttbr_table[l0_index];
    if ((l0_entry & TBL_ENTRY_VALID) == 0) {
        return -1;
    }

    uint64_t *l0_page_table = (uint64_t *)vmap->physical_to_virtual((uint64_t)(l0_entry & TBL_ENTRY_ADDR_MASK));
    uint64_t l1_index = L1_INDEX(virtual_address);
    uint64_t l1_entry = l0_page_table[l1_index];
    if ((l1_entry & TBL_ENTRY_VALID) == 0) {
        return -1;
    }
    
    // Block entry, return the physical address.
    if ((l1_entry & TBL_ENTRY_TABLE) == 0) {
        *physical_address = (l1_entry & TBL_ENTRY_L1_BLOCK_ADDR_MASK) | (virtual_address & (TBL_L1_BLOCK_SIZE - 1));
        return 0;
    }

    uint64_t *l1_page_table = (uint64_t *)vmap->physical_to_virtual((uint64_t)(l1_entry & TBL_ENTRY_ADDR_MASK));
    uint64_t l2_index = L2_INDEX(virtual_address);
    uint64_t l2_entry = l1_page_table[l2_index];
    if ((l2_entry & TBL_ENTRY_VALID) == 0) {
        return -1;
    }

    // Block entry, return the physical address.
    if ((l2_entry & TBL_ENTRY_TABLE) == 0) {
        *physical_address = (l2_entry & TBL_ENTRY_L2_BLOCK_ADDR_MASK) | (virtual_address & (TBL_L2_BLOCK_SIZE - 1));
        return 0;
    }

    uint64_t *l2_page_table = (uint64_t *)vmap->physical_to_virtual((uint64_t)(l2_entry & TBL_ENTRY_ADDR_MASK));

    uint64_t l3_index = L3_INDEX(virtual_address);
    uint64_t l3_entry = l2_page_table[l3_index];
    if ((l3_entry & TBL_ENTRY_VALID) == 0) {
        return -1;
    }

    // Page entry, return the physical address.
    *physical_address = (l3_entry & TBL_ENTRY_ADDR_MASK) | (virtual_address & (MMAP_GRANULE_SIZE - 1));
    return 0;
}

uint8_t vmap_is_active(
    vmap_t *vmap
) {
    uint64_t ttbr0 = 0;
    __asm__("mrs %0, ttbr0_el1" : "=r" (ttbr0));

    if ((ttbr0 >> 1) == (vmap->root_page_table >> 1)) {
        return 1;
    }

    uint64_t ttbr1 = 0;
    __asm__("mrs %0, ttbr1_el1" : "=r" (ttbr1));
    if ((ttbr1 >> 1) == (vmap->root_page_table >> 1)) {
        return 1;
    }

    return 0;
}
