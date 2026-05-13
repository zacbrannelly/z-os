#include "block_allocator.h"

#include "assert.h"
#include "memory.h"
#include "vmap.h"
#include "page_alloc.h"

#include <stddef.h>

#define BLOCK_ALLOCATOR_DEFAULT_ORDER PAGE_ALLOC_ORDER_4MB
#define BLOCK_ALLOCATOR_DEFAULT_SIZE (4 * 1024 * 1024) // 4MB
#define BLOCK_ALLOCATOR_BLOCK_MAGIC 0x56 // 'V'

typedef struct block_header_t {
    uint8_t magic;
    uint8_t is_free;
    uint64_t size;
    uint64_t offset;
    struct block_header_t *prev_ptr;
    struct block_header_t *next_ptr;
    struct block_header_t *free_list_next_ptr;
    struct block_header_t *free_list_prev_ptr;
} block_header_t;

static void add_block_to_free_list(block_allocator_t *allocator, block_header_t *block_header) {
    block_header->free_list_prev_ptr = NULL;
    block_header->free_list_next_ptr = allocator->free_list_head;
    allocator->free_list_head = block_header;

    if (block_header->free_list_next_ptr != NULL) {
        block_header->free_list_next_ptr->free_list_prev_ptr = block_header;
    }
}

static void remove_block_from_free_list(block_allocator_t *allocator, block_header_t *block_header) {
    block_header_t *free_list_prev_ptr = block_header->free_list_prev_ptr;
    block_header_t *free_list_next_ptr = block_header->free_list_next_ptr;

    if (free_list_prev_ptr != NULL) {
        free_list_prev_ptr->free_list_next_ptr = free_list_next_ptr;
    } else {
        allocator->free_list_head = free_list_next_ptr;
    }

    if (free_list_next_ptr != NULL) {
        free_list_next_ptr->free_list_prev_ptr = free_list_prev_ptr;
    }

    block_header->free_list_next_ptr = NULL;
    block_header->free_list_prev_ptr = NULL;
}

static void remove_block_from_block_list(block_allocator_t *allocator, block_header_t *block_header) {
    block_header_t *prev_block_header = block_header->prev_ptr;
    block_header_t *next_block_header = block_header->next_ptr;

    if (prev_block_header != NULL) {
        prev_block_header->next_ptr = next_block_header;
    } else {
        allocator->block_list_head = next_block_header;
    }

    if (next_block_header != NULL) {
        next_block_header->prev_ptr = prev_block_header;
    }

    block_header->next_ptr = NULL;
    block_header->prev_ptr = NULL;
}

static block_header_t *find_free_block(block_allocator_t *allocator, uint64_t size) {
    block_header_t *block_header = allocator->free_list_head;

    while (block_header != NULL) {
        if (block_header->size >= size) {
            break;
        }
        block_header = block_header->free_list_next_ptr;
    }

    return block_header;
}

static block_header_t *get_last_block_header(block_allocator_t *allocator) {
    block_header_t *block_header = allocator->block_list_head;
    while (block_header != NULL) {
        if (block_header->next_ptr == NULL) {
            break;
        }
        block_header = block_header->next_ptr;
    }
    return block_header;
}

int block_allocator_init(
    block_allocator_t *allocator,
    vmap_t *address_space,
    uint64_t virtual_base,
    uint64_t page_flags
) {
    // Address space is assumed to be active.
    assert(vmap_is_active(address_space));

    memory_set(allocator, 0, sizeof(block_allocator_t));
    allocator->address_space = address_space;
    allocator->virtual_base = virtual_base;
    allocator->page_flags = page_flags;

    uint64_t page_address = 0;
    if (page_alloc_block(BLOCK_ALLOCATOR_DEFAULT_ORDER, &page_address) < 0) {
        return -1;
    }
    if (vmap_map_range(
        address_space,
        virtual_base,
        virtual_base + BLOCK_ALLOCATOR_DEFAULT_SIZE,
        page_address,
        page_flags) < 0) {
        return -1;
    }

    allocator->block_list_head = (block_header_t *)virtual_base;
    allocator->free_list_head = (block_header_t *)virtual_base;

    block_header_t *block_header = (block_header_t *)virtual_base;
    block_header->magic = BLOCK_ALLOCATOR_BLOCK_MAGIC;
    block_header->offset = sizeof(block_header_t);
    block_header->is_free = 1;
    block_header->size = BLOCK_ALLOCATOR_DEFAULT_SIZE - sizeof(block_header_t);
    block_header->prev_ptr = NULL;
    block_header->next_ptr = NULL;
    block_header->free_list_next_ptr = NULL;
    block_header->free_list_prev_ptr = NULL;

    return 0;
}

static block_header_t *find_or_create_free_block(block_allocator_t *allocator, uint64_t size) {
    block_header_t *block_header = find_free_block(allocator, size);
    if (block_header != NULL) {
        return block_header;
    }

    block_header_t *last_block = get_last_block_header(allocator);
    uint64_t offset = 0;
    if (last_block != NULL) {
        offset = last_block->offset + last_block->size + sizeof(block_header_t);
    } else {
        offset = sizeof(block_header_t);
    }

    uint64_t num_pages = (size + sizeof(block_header_t)) / PAGE_SIZE;
    if ((size + sizeof(block_header_t)) % PAGE_SIZE != 0) {
        num_pages++;
    }

    for (uint64_t i = 0; i < num_pages; i++) {
        uint64_t page_address = 0;
        if (page_alloc_block(PAGE_ALLOC_ORDER_4KB, &page_address) < 0) {
            // TODO: Gracefully handle this error, make sure it doesn't leave the system in an invalid state.
            return NULL;
        }

        uint64_t page_virtual_address = allocator->virtual_base + offset - sizeof(block_header_t) + i * PAGE_SIZE;
        if (vmap_map_page(
            allocator->address_space,
            page_address,
            page_virtual_address,
            allocator->page_flags
        ) < 0) {
            // TODO: Gracefully handle this error, make sure it doesn't leave the system in an invalid state.
            return NULL;
        }
    }

    // Build the new block header.
    block_header_t *new_block_header = (block_header_t *)(allocator->virtual_base + offset - sizeof(block_header_t));
    new_block_header->magic = BLOCK_ALLOCATOR_BLOCK_MAGIC;
    new_block_header->offset = offset;
    new_block_header->is_free = 1;
    new_block_header->size = num_pages * PAGE_SIZE - sizeof(block_header_t);
    
    if (last_block != NULL) {
        // Append new block to the end of the block list.
        last_block->next_ptr = new_block_header;
        new_block_header->prev_ptr = last_block;
        new_block_header->next_ptr = NULL;
    } else {
        // Create first block in the block list.
        allocator->block_list_head = new_block_header;
        new_block_header->prev_ptr = NULL;
        new_block_header->next_ptr = NULL;
    }

    // Add the new block to the free list.
    add_block_to_free_list(allocator, new_block_header);

    return new_block_header;
}

static void split_block(block_allocator_t *allocator, block_header_t *block_header, uint64_t size) {
    uint64_t delta = block_header->size - size;

    // Skip if the block is too small to split.
    if (delta <= sizeof(block_header_t)) {
        return;
    }

    // Split the block into two.
    // [BLOCK] transforms into [BLOCK][REMAINDER BLOCK]
    block_header->size = size;

    block_header_t *next_ptr = block_header->next_ptr;
    block_header->next_ptr = (block_header_t *)((uint64_t)block_header + size + sizeof(block_header_t));
    block_header->next_ptr->magic = BLOCK_ALLOCATOR_BLOCK_MAGIC;
    block_header->next_ptr->offset = block_header->offset + size + sizeof(block_header_t);
    block_header->next_ptr->is_free = 1;
    block_header->next_ptr->size = delta - sizeof(block_header_t);
    block_header->next_ptr->prev_ptr = block_header;
    block_header->next_ptr->next_ptr = next_ptr;

    if (next_ptr != NULL) {
        next_ptr->prev_ptr = block_header->next_ptr;
    }

    // Add the REMAINDER BLOCK to the free list.
    add_block_to_free_list(allocator, block_header->next_ptr);
}

// Allocate a block of memory.
void *block_allocator_allocate(block_allocator_t *allocator, uint64_t size) {
    // Address space is assumed to be active.
    assert(vmap_is_active(allocator->address_space));

    block_header_t *block_header = find_or_create_free_block(allocator, size);
    if (block_header == NULL) {
        return NULL;
    }

    // Optionally split the block into two.
    split_block(allocator, block_header, size);

    // Remove the block from the free list.
    block_header->is_free = 0;
    remove_block_from_free_list(allocator, block_header);

    return (void *)(allocator->virtual_base + block_header->offset);
}

// Free a block of memory.
void block_allocator_free(block_allocator_t *allocator, void *address) {
    if (address == NULL) {
        return;
    }

    // Address space is assumed to be active.
    assert(vmap_is_active(allocator->address_space));

    // Check if the block header is valid.
    block_header_t *block_header = (block_header_t *)(address - sizeof(block_header_t));
    if (block_header->magic != BLOCK_ALLOCATOR_BLOCK_MAGIC || block_header->is_free) {
        return;
    }

    block_header_t *prev_block_header = block_header->prev_ptr;
    block_header_t *next_block_header = block_header->next_ptr;

    uint8_t is_prev_free = prev_block_header != NULL ? prev_block_header->is_free : 0;
    uint8_t is_next_free = next_block_header != NULL ? next_block_header->is_free : 0;

    if (is_prev_free && is_next_free) {
        // [PREV BLOCK] -> [BLOCK] -> [NEXT BLOCK]
        prev_block_header->next_ptr = next_block_header->next_ptr;
        prev_block_header->size += 2 * sizeof(block_header_t) + block_header->size + next_block_header->size;

        // Remove the NEXT BLOCK from both lists.
        remove_block_from_free_list(allocator, next_block_header);
        remove_block_from_block_list(allocator, next_block_header);

        // Remove the BLOCK from the block list.
        remove_block_from_block_list(allocator, block_header);
    } else if (is_prev_free) {
        // [PREV BLOCK] -> [BLOCK]
        prev_block_header->next_ptr = block_header->next_ptr;
        prev_block_header->size += sizeof(block_header_t) + block_header->size;

        // Remove the BLOCK from the block list.
        remove_block_from_block_list(allocator, block_header);
    } else if (is_next_free) {
        // [BLOCK] -> [NEXT BLOCK]
        block_header->next_ptr = next_block_header->next_ptr;
        block_header->size += sizeof(block_header_t) + next_block_header->size;

        // Remove the NEXT BLOCK from both lists.
        remove_block_from_block_list(allocator, next_block_header);
        remove_block_from_free_list(allocator, next_block_header);

        // Add the BLOCK to the free list.
        block_header->is_free = 1;
        add_block_to_free_list(allocator, block_header);
    } else {
        // [BLOCK]
        block_header->is_free = 1;
        add_block_to_free_list(allocator, block_header);
    }
}
