#include "kmalloc.h"
#include "mmap.h"
#include "vmap.h"
#include "block_allocator.h"

#include <stddef.h>

#define KERNEL_HEAP_VIRTUAL_BASE 0xffffffffa0000000ULL
#define KERNEL_HEAP_PAGE_FLAGS PAGE_FLAG_NORMAL_MEMORY | PAGE_FLAG_EL1_RW | PAGE_FLAG_NX

static block_allocator_t g_kernel_heap;
static vmap_t g_kernel_address_space;

int kernel_heap_init(void) {
    if (mmap_build_vmap(&g_kernel_address_space, KERNEL_HEAP_VIRTUAL_BASE) < 0) {
        return -1;
    }

    return block_allocator_init(
        &g_kernel_heap,
        &g_kernel_address_space,
        KERNEL_HEAP_VIRTUAL_BASE,
        KERNEL_HEAP_PAGE_FLAGS
    );
}

void *kmalloc(uint64_t size) {
    return block_allocator_allocate(&g_kernel_heap, size);
}

void kfree(void *address) {
    block_allocator_free(&g_kernel_heap, address);
}
