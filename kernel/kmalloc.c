#include "kmalloc.h"
#include "page_alloc.h"
#include "mmap.h"

#include <stddef.h>

#define KERNEL_HEAP_VIRTUAL_BASE 0xffffffffa0000000ULL
#define KERNEL_HEAP_INITIAL_ORDER PAGE_ALLOC_ORDER_4MB
#define KERNEL_HEAP_INITIAL_SIZE (4 * 1024 * 1024)

#define KERNEL_HEAP_BLOCK_MAGIC 0x56 // 'V'

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

typedef struct kernel_heap_t {
    uint64_t virtual_base;
    block_header_t *block_list_head;
    block_header_t *free_list_head;
} kernel_heap_t;

static kernel_heap_t g_kernel_heap = {
    .virtual_base = KERNEL_HEAP_VIRTUAL_BASE,
    .block_list_head = NULL,
    .free_list_head = NULL,
};

static void add_block_to_free_list(block_header_t *block_header) {
    block_header->free_list_prev_ptr = NULL;
    block_header->free_list_next_ptr = g_kernel_heap.free_list_head;
    g_kernel_heap.free_list_head = block_header;

    if (block_header->free_list_next_ptr != NULL) {
        block_header->free_list_next_ptr->free_list_prev_ptr = block_header;
    }
}

static void remove_block_from_free_list(block_header_t *block_header) {
    block_header_t *free_list_prev_ptr = block_header->free_list_prev_ptr;
    block_header_t *free_list_next_ptr = block_header->free_list_next_ptr;

    if (free_list_prev_ptr != NULL) {
        free_list_prev_ptr->free_list_next_ptr = free_list_next_ptr;
    } else {
        g_kernel_heap.free_list_head = free_list_next_ptr;
    }

    if (free_list_next_ptr != NULL) {
        free_list_next_ptr->free_list_prev_ptr = free_list_prev_ptr;
    }

    block_header->free_list_next_ptr = NULL;
    block_header->free_list_prev_ptr = NULL;
}

static void remove_block_from_block_list(block_header_t *block_header) {
    block_header_t *prev_block_header = block_header->prev_ptr;
    block_header_t *next_block_header = block_header->next_ptr;

    if (prev_block_header != NULL) {
        prev_block_header->next_ptr = next_block_header;
    } else {
        g_kernel_heap.block_list_head = next_block_header;
    }

    if (next_block_header != NULL) {
        next_block_header->prev_ptr = prev_block_header;
    }

    block_header->next_ptr = NULL;
    block_header->prev_ptr = NULL;
}

int kernel_heap_init(void) {
    uint64_t page_address = 0;
    if (page_alloc_block(KERNEL_HEAP_INITIAL_ORDER, &page_address) < 0) {
        return -1;
    }
    if (mmap_map_range(
        KERNEL_HEAP_VIRTUAL_BASE,
        KERNEL_HEAP_VIRTUAL_BASE + KERNEL_HEAP_INITIAL_SIZE,
        page_address,
        PAGE_FLAG_NX | PAGE_FLAG_EL0_EL1_RW | PAGE_FLAG_NORMAL_MEMORY
    ) < 0) {
        return -1;
    }

    g_kernel_heap.block_list_head = (block_header_t *)KERNEL_HEAP_VIRTUAL_BASE;
    g_kernel_heap.free_list_head = (block_header_t *)KERNEL_HEAP_VIRTUAL_BASE;

    block_header_t *block_header = (block_header_t *)KERNEL_HEAP_VIRTUAL_BASE;
    block_header->magic = KERNEL_HEAP_BLOCK_MAGIC;
    block_header->offset = sizeof(block_header_t);
    block_header->is_free = 1;
    block_header->size = KERNEL_HEAP_INITIAL_SIZE - sizeof(block_header_t);
    block_header->prev_ptr = NULL;
    block_header->next_ptr = NULL;
    block_header->free_list_next_ptr = NULL;
    block_header->free_list_prev_ptr = NULL;

    return 0;
}

static block_header_t *find_free_block(uint64_t size) {
    block_header_t *block_header = g_kernel_heap.free_list_head;

    while (block_header != NULL) {
        if (block_header->size >= size) {
            break;
        }
        block_header = block_header->free_list_next_ptr;
    }

    return block_header;
}

static block_header_t *get_last_block_header(void) {
    block_header_t *block_header = g_kernel_heap.block_list_head;
    while (block_header != NULL) {
        if (block_header->next_ptr == NULL) {
            break;
        }
        block_header = block_header->next_ptr;
    }
    return block_header;
}

void *kmalloc(uint64_t size) {
    if (size == 0) {
        return NULL;
    }

    block_header_t *block_header = find_free_block(size);
    if (block_header == NULL) {
        block_header_t *last_block = get_last_block_header();
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

            uint64_t page_virtual_address = KERNEL_HEAP_VIRTUAL_BASE + offset - sizeof(block_header_t) + i * PAGE_SIZE;
            if (mmap_map_range(
                page_virtual_address,
                page_virtual_address + PAGE_SIZE,
                page_address,
                PAGE_FLAG_NX | PAGE_FLAG_EL0_EL1_RW | PAGE_FLAG_NORMAL_MEMORY
            ) < 0) {
                // TODO: Gracefully handle this error, make sure it doesn't leave the system in an invalid state.
                return NULL;
            }
        }

        // Build the new block header.
        block_header_t *new_block_header = (block_header_t *)(KERNEL_HEAP_VIRTUAL_BASE + offset - sizeof(block_header_t));
        new_block_header->magic = KERNEL_HEAP_BLOCK_MAGIC;
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
            g_kernel_heap.block_list_head = new_block_header;
            new_block_header->prev_ptr = NULL;
            new_block_header->next_ptr = NULL;
        }

        // Add the new block to the free list.
        add_block_to_free_list(new_block_header);

        block_header = new_block_header;
    }

    uint64_t delta = block_header->size - size;
    if (delta > sizeof(block_header_t)) {
        // Split the block into two.
        // [BLOCK] transforms into [BLOCK][REMAINDER BLOCK]
        block_header->size = size;

        block_header_t *next_ptr = block_header->next_ptr;

        block_header->next_ptr = (block_header_t *)((uint64_t)block_header + size + sizeof(block_header_t));
        block_header->next_ptr->magic = KERNEL_HEAP_BLOCK_MAGIC;
        block_header->next_ptr->offset = block_header->offset + size + sizeof(block_header_t);
        block_header->next_ptr->is_free = 1;
        block_header->next_ptr->size = delta - sizeof(block_header_t);
        block_header->next_ptr->prev_ptr = block_header;
        block_header->next_ptr->next_ptr = next_ptr;

        if (next_ptr != NULL) {
            next_ptr->prev_ptr = block_header->next_ptr;
        }

        // Add the REMAINDER BLOCK to the free list.
        add_block_to_free_list(block_header->next_ptr);
    }

    // Remove the block from the free list.
    block_header->is_free = 0;
    remove_block_from_free_list(block_header);

    return (void *)(KERNEL_HEAP_VIRTUAL_BASE + block_header->offset);
}

void kfree(void *address) {
    if (address == NULL) {
        return;
    }

    // Check if the block header is valid.
    block_header_t *block_header = (block_header_t *)(address - sizeof(block_header_t));
    if (block_header->magic != KERNEL_HEAP_BLOCK_MAGIC || block_header->is_free) {
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
        remove_block_from_free_list(next_block_header);
        remove_block_from_block_list(next_block_header);

        // Remove the BLOCK from the block list.
        remove_block_from_block_list(block_header);
    } else if (is_prev_free) {
        // [PREV BLOCK] -> [BLOCK]
        prev_block_header->next_ptr = block_header->next_ptr;
        prev_block_header->size += sizeof(block_header_t) + block_header->size;

        // Remove the BLOCK from the block list.
        remove_block_from_block_list(block_header);
    } else if (is_next_free) {
        // [BLOCK] -> [NEXT BLOCK]
        block_header->next_ptr = next_block_header->next_ptr;
        block_header->size += sizeof(block_header_t) + next_block_header->size;

        // Remove the NEXT BLOCK from both lists.
        remove_block_from_block_list(next_block_header);
        remove_block_from_free_list(next_block_header);

        // Add the BLOCK to the free list.
        block_header->is_free = 1;
        add_block_to_free_list(block_header);
    } else {
        // [BLOCK]
        block_header->is_free = 1;
        add_block_to_free_list(block_header);
    }
}
