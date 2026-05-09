#pragma once

#include "efi_memory_map.h"

#include <stdint.h>

#define MMAP_GRANULE_SIZE 4096

#define TBL_ENTRY_VALID 0x1ULL
#define TBL_ENTRY_TABLE 0x2ULL
#define TBL_ENTRY_BLOCK 0x0ULL
#define TBL_ENTRY_ADDR_MASK 0x0000FFFFFFFFF000ULL
#define TBL_ENTRY_L1_BLOCK_ADDR_MASK (TBL_ENTRY_ADDR_MASK & ~((1ULL << 30) - 1))
#define TBL_ENTRY_L2_BLOCK_ADDR_MASK (TBL_ENTRY_ADDR_MASK & ~((1ULL << 21) - 1))

#define TBL_L1_BLOCK_SIZE (1ULL << 30) // 1GB
#define TBL_L2_BLOCK_SIZE (1ULL << 21) // 2MiB

#define PAGE_FLAG_ACCESS (1ULL << 10)
#define PAGE_FLAG_INNER_SHARABLE (3ULL << 8)
#define PAGE_FLAG_NON_SHARABLE (0ULL << 8)
#define PAGE_FLAG_UXN (1ULL << 54) // UXN = Unprivileged Execute Never
#define PAGE_FLAG_PXN (1ULL << 53) // PXN = Privileged Execute Never
#define PAGE_FLAG_NX PAGE_FLAG_PXN | PAGE_FLAG_UXN
#define PAGE_FLAG_EL0_EL1_RO (0x3ULL << 6) // Read Only (for EL0 and EL1)
#define PAGE_FLAG_EL1_RO (0x2ULL << 6) // Privilged Read Only (for EL1)
#define PAGE_FLAG_EL0_EL1_RW (0x1ULL << 6) // Read/Write (for EL0 and EL1)
#define PAGE_FLAG_EL1_RW (0x0ULL << 6) // Privilged Read/Write (for EL1)
#define PAGE_FLAG_MAIR_ATTR(attr) (attr << 2)

#define DEVICE_MEMORY_ATTR_INDEX 0x0
#define NORMAL_MEMORY_NC_ATTR_INDEX 0x1
#define NORMAL_MEMORY_ATTR_INDEX 0x3

#define PAGE_FLAG_NORMAL_MEMORY PAGE_FLAG_MAIR_ATTR(NORMAL_MEMORY_ATTR_INDEX)
#define PAGE_FLAG_DEVICE_MEMORY PAGE_FLAG_MAIR_ATTR(DEVICE_MEMORY_ATTR_INDEX)
#define PAGE_FLAG_NORMAL_MEMORY_NC PAGE_FLAG_MAIR_ATTR(NORMAL_MEMORY_NC_ATTR_INDEX)

#define MMAP_MEMORY_MAP_RESERVED_SIZE (1ULL << 20) // 1MiB (256 pages)
#define PAGE_TABLE_RESERVED_SIZE (1ULL << 23) // 8MiB (2048 pages)

typedef enum mmap_memory_type_t {
    MMAP_MEMORY_TYPE_USABLE,
    MMAP_MEMORY_TYPE_RESERVED,
} mmap_memory_type_t;

typedef struct mmap_memory_descriptor_t {
    uint64_t physical_start_address;
    uint64_t number_of_pages;
    mmap_memory_type_t type;
} mmap_memory_descriptor_t;

int mmap_init(
    efi_memory_descriptor_t *efi_memory_map,
    uint64_t efi_memory_map_size,
    uint64_t efi_memory_map_descriptor_size
);

int mmap_get_memory_map(
    mmap_memory_descriptor_t **out_memory_map,
    uint64_t *out_memory_map_count
);

int mmap_apply_mappings(void);

int mmap_virtual_to_physical(
    uint64_t virtual_address,
    uint64_t *physical_address
);

int mmap_physical_to_virtual(
    uint64_t physical_address,
    uint64_t *virtual_address
);

// Maps a range of L2 blocks (2MiB each)
int mmap_map_range_l2_block(
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
);

// Maps a single L2 block (2MiB)
int mmap_map_l2_block(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t page_flags
);

// Maps a range of L1 blocks (1GB each)
int mmap_map_range_l1_block(
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
);
// Maps a single L1 block (1GB)
int mmap_map_l1_block(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t page_flags
);

int mmap_map_range(
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
);

int mmap_map_page(
    uint64_t physical_address,
    uint64_t virtual_address,
    uint64_t page_flags
);

void mmap_debug_print(
    efi_memory_descriptor_t *efi_memory_map,
    uint64_t efi_memory_map_size,
    uint64_t efi_memory_map_descriptor_size
);
