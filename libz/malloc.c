#include "malloc.h"
#include "block_allocator.h"
#include "mmap.h"

#define MALLOC_PAGE_FLAGS MAP_PRIVATE | MAP_ANONYMOUS | MAP_READ | MAP_WRITE

static block_allocator_t g_block_allocator;

void malloc_init(void) {
    block_allocator_init(&g_block_allocator, MALLOC_PAGE_FLAGS);
}

void *malloc(uint64_t size) {
    return block_allocator_allocate(&g_block_allocator, size);
}

void free(void *ptr) {
    block_allocator_free(&g_block_allocator, ptr);
}
