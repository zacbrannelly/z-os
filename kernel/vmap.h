#pragma once

#include <stdint.h>

typedef uint64_t (*vmap_alloc_page_fn_t)(void);
typedef uint64_t (*vmap_physical_to_virtual_fn_t)(uint64_t physical_address);

typedef enum vmap_destination_t {
    VMAP_DESTINATION_USER,
    VMAP_DESTINATION_KERNEL,
} vmap_destination_t;

typedef struct vmap_t {
    uint64_t root_page_table;
    vmap_alloc_page_fn_t alloc_page;
    vmap_physical_to_virtual_fn_t physical_to_virtual;
} vmap_t;

int vmap_init(
    vmap_t *vmap,
    vmap_alloc_page_fn_t alloc_page,
    vmap_physical_to_virtual_fn_t physical_to_virtual
);

// Maps a range of L2 blocks (2MiB each)
int vmap_map_range_l2_block(
    vmap_t *vmap,
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
);

// Maps a single L2 block (2MiB)
int vmap_map_l2_block(
    vmap_t *vmap,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t page_flags
);

// Maps a range of L1 blocks (1GB each)
int vmap_map_range_l1_block(
    vmap_t *vmap,
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
);

// Maps a single L1 block (1GB)
int vmap_map_l1_block(
    vmap_t *vmap,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t page_flags
);

int vmap_map_range(
    vmap_t *vmap,
    uint64_t virtual_start_address,
    uint64_t virtual_end_address,
    uint64_t physical_start_address,
    uint64_t page_flags
);

int vmap_map_page(
    vmap_t *vmap,
    uint64_t physical_address,
    uint64_t virtual_address,
    uint64_t page_flags
);

int vmap_apply_table(
    vmap_t *vmap,
    vmap_destination_t destination
);

int vmap_virtual_to_physical(
    vmap_t *vmap,
    uint64_t virtual_address,
    uint64_t *physical_address
);

uint8_t vmap_is_active(
    vmap_t *vmap
);
