#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct vmap_t vmap_t;
typedef struct block_header_t block_header_t;

typedef struct block_allocator_t {
    vmap_t *address_space;
    uint64_t virtual_base;
    uint64_t page_flags;

    block_header_t *block_list_head;
    block_header_t *free_list_head;
} block_allocator_t;

int block_allocator_init(
    block_allocator_t *allocator,
    vmap_t *address_space,
    uint64_t virtual_base,
    uint64_t page_flags
);

// Allocate a block of memory.
void *block_allocator_allocate(block_allocator_t *allocator, uint64_t size);

// Free a block of memory.
void block_allocator_free(block_allocator_t *allocator, void *address);
