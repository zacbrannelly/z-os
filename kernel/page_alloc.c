#include "page_alloc.h"
#include "console.h"

#include <stddef.h>

#define PAGE_SHIFT 12 // 2^12 = 4096, applying the shift gets the page frame index.

#define MAX_ORDER 10 // 2^10 = 1024 pages (4MB)
#define MAX_BLOCK_SIZE (1ULL << MAX_ORDER)
#define MAX_TOTAL_PAGES 0x1000000 // 64GB

#define PAGE_BITMAP_ORDER_MASK (0xFF << 3)
#define PAGE_BITMAP_IS_FREE_MASK 0x4
#define PAGE_BITMAP_IS_HEAD_MASK 0x2
#define PAGE_BITMAP_VALID_MASK 0x1

typedef struct free_page_t {
    struct free_page_t *next_page;
} free_page_t;

// Holds the heads of the free linked lists for each order.
static free_page_t* free_lists[MAX_ORDER + 1];

// Bitmap to track the allocation status of each page.
// [order (8 bits)][is_free (1 bit)][is_head (1 bit)][valid (1 bit)]
static uint16_t page_bitmap[MAX_TOTAL_PAGES];

// Finds the PFN of the buddy page for a given page frame number and order.
static uint64_t calculate_buddy_page_frame_number(uint64_t page_frame_number, uint8_t order) {
    return page_frame_number ^ (1ULL << order);
}

static int free_list_push(uint64_t order, uint64_t page_frame_number) {
    if (order > MAX_ORDER) {
        return -1;
    }

    if (page_frame_number >= MAX_TOTAL_PAGES) {
        return -1;
    }

    free_page_t *free_page = (free_page_t *)((uint64_t)page_frame_number << PAGE_SHIFT);
    free_page->next_page = free_lists[order];
    free_lists[order] = free_page;

    page_bitmap[page_frame_number] = (
        (order << 3) |
        PAGE_BITMAP_IS_FREE_MASK |
        PAGE_BITMAP_VALID_MASK |
        PAGE_BITMAP_IS_HEAD_MASK
    );

    return 0;
}

static int free_list_pop(uint8_t order, uint64_t *found_page_frame_number) {
    free_page_t *free_page = free_lists[order];
    if (free_page == NULL) {
        return -1;
    }

    uint64_t page_frame_number = (uint64_t)free_page >> PAGE_SHIFT;
    page_bitmap[page_frame_number] = (
        (order << 3) |
        PAGE_BITMAP_IS_HEAD_MASK |
        PAGE_BITMAP_VALID_MASK
    );

    free_page_t *next_page = free_page->next_page;
    free_lists[order] = next_page;

    *found_page_frame_number = page_frame_number;
    return 0;
}

int page_alloc_init(
    mmap_memory_descriptor_t *memory_map,
    uint64_t memory_map_count
) {
    for (int i = 0; i <= MAX_ORDER; i++) {
        free_lists[i] = NULL;
    }

    for (int i = 0; i < MAX_TOTAL_PAGES; i++) {
        page_bitmap[i] = 0;
    }

    for (int i = 0; i < memory_map_count; i++) {
        mmap_memory_descriptor_t *descriptor = &memory_map[i];
        if (descriptor->type != MMAP_MEMORY_TYPE_USABLE) {
            continue;
        }

        int64_t remaining_pages = (int64_t)descriptor->number_of_pages;
        uint64_t start_page_frame_idx = descriptor->physical_start_address >> PAGE_SHIFT;
        free_page_t *free_page = (free_page_t *)descriptor->physical_start_address;

        while (remaining_pages > 0) {
            // Calculate largest order that fits in the remaining space.
            uint8_t order = MAX_ORDER;
            while (order > 0) {
                uint64_t block_pages = 1ULL << order;
                if (block_pages <= remaining_pages && (start_page_frame_idx % block_pages) == 0) {
                    break;
                }
                order--;
            }

            // Append the free page to the free linked list.
            if (free_list_push(order, start_page_frame_idx) < 0) {
                return -1;
            }

            // Prepare for the next free page.
            uint64_t block_pages = 1ULL << order;
            remaining_pages -= block_pages;
            start_page_frame_idx += block_pages;
            free_page = (free_page_t *)((uint64_t)free_page + block_pages * PAGE_SIZE);
        }
    }

    return 0;
}

uint64_t page_alloc_block_size(uint8_t order) {
    return 1ULL << order;
}

int page_alloc_block(uint8_t order, uint64_t *physical_start_address) {
    int current_order = order;
    uint64_t page_frame_number = 0;

    // Try find a free block that is big enough to fit the requested order.
    while (current_order <= MAX_ORDER) {
        uint64_t found_page_frame_number = 0;
        if (free_list_pop(current_order, &page_frame_number) == 0) {
            // Found a free block 
            break;
        }
        current_order++;
    }

    if (current_order > MAX_ORDER) {
        // Out of memory.
        return -1;
    }

    // Reduce order until it matches the requested order.
    // Splitting the block into smaller blocks and pushing them to the free list.
    while (current_order > order) {
        current_order--;

        // Split the block into half and push the other half to the free list.
        uint64_t half_block_size = page_alloc_block_size(current_order);
        if (free_list_push(current_order, page_frame_number + half_block_size) < 0) {
            return -1;
        }
    }

    page_bitmap[page_frame_number] = (
        (order << 3) |
        PAGE_BITMAP_IS_HEAD_MASK |
        PAGE_BITMAP_VALID_MASK
    );

    *physical_start_address = page_frame_number << PAGE_SHIFT;
    return 0;
}

int page_alloc_free(uint64_t physical_start_address, uint8_t order) {
    uint8_t current_order = order;
    uint64_t page_frame_number = physical_start_address >> PAGE_SHIFT;

    // Attempt to iteratively merge the page with its buddy page, to build up a larger block of free contiguous pages.
    while (current_order < MAX_ORDER) {
        uint64_t buddy_page_frame_number = calculate_buddy_page_frame_number(page_frame_number, current_order);
        if (buddy_page_frame_number >= MAX_TOTAL_PAGES) {
            break;
        }

        // If the buddy page is not available, then there is no merge possible.
        uint16_t buddy_page_bitmap = page_bitmap[buddy_page_frame_number];
        int merge_possible = (
            (buddy_page_bitmap & PAGE_BITMAP_VALID_MASK) > 0 &&
            (buddy_page_bitmap & PAGE_BITMAP_IS_FREE_MASK) > 0 &&
            (buddy_page_bitmap & PAGE_BITMAP_IS_HEAD_MASK) > 0 &&
            (buddy_page_bitmap >> 3) == current_order
        );
        if (!merge_possible) {
            break;
        }

        // Remove the buddy page from the free list.
        free_page_t *current_page = free_lists[current_order];
        while (current_page != NULL) {
            uint64_t next_page_frame_number = (uint64_t)current_page->next_page >> PAGE_SHIFT;
            if (next_page_frame_number == buddy_page_frame_number) {
                // Found the buddy page, so we remove it from the free list.
                current_page->next_page = current_page->next_page->next_page;
                break;
            }
        }

        // Make sure the buddy page is not free anymore (as it is part of a larger block now).
        page_bitmap[buddy_page_frame_number] &= ~PAGE_BITMAP_IS_FREE_MASK;

        current_order++;
        page_frame_number = page_frame_number > buddy_page_frame_number 
            ? buddy_page_frame_number 
            : page_frame_number;
    }

    // Push the larger block to the free list.
    return free_list_push(current_order, page_frame_number);
}
