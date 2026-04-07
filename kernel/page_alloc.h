#pragma once

#include <stdint.h>
#include "mmap.h"

#define PAGE_SIZE 4096
#define PAGE_ALLOC_ORDER_4KB 0 // Order 0 = 4kb page.
#define PAGE_ALLOC_ORDER_8KB 1 // Order 1 = 8kb page.
#define PAGE_ALLOC_ORDER_16KB 2 // Order 2 = 16kb page.
#define PAGE_ALLOC_ORDER_32KB 3 // Order 3 = 32kb page.
#define PAGE_ALLOC_ORDER_64KB 4 // Order 4 = 64kb page.
#define PAGE_ALLOC_ORDER_128KB 5 // Order 5 = 128kb page.
#define PAGE_ALLOC_ORDER_256KB 6 // Order 6 = 256kb page.
#define PAGE_ALLOC_ORDER_512KB 7 // Order 7 = 512kb page.
#define PAGE_ALLOC_ORDER_1MB 8 // Order 8 = 1mb page.
#define PAGE_ALLOC_ORDER_2MB 9 // Order 9 = 2mb page.
#define PAGE_ALLOC_ORDER_4MB 10 // Order 10 = 4mb page.

int page_alloc_init(
    mmap_memory_descriptor_t *memory_map,
    uint64_t memory_map_count
);

// Convert the order to the number of pages.
uint64_t page_alloc_block_size(uint8_t order);

// Allocate a block of contiguous pages.
int page_alloc_block(uint8_t requested_order, uint64_t *physical_address);

// Free a block of contiguous pages.
int page_alloc_free(uint64_t physical_address, uint8_t order);
