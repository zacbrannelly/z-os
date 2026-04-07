#include "bump_allocator.h"

int bump_allocator_make_invalid(bump_allocator_t *allocator) {
    allocator->memory_start = 0;
    allocator->memory_end = 0;
    allocator->allocated_size = 0;
    allocator->remaining_size = 0;
    return 0;
}

int bump_allocator_invalid(bump_allocator_t *allocator) {
    return allocator->memory_start == 0 || allocator->memory_end == 0;
}

int bump_allocator_init(bump_allocator_t *allocator, uint64_t memory_start, uint64_t memory_end) {
    allocator->memory_start = memory_start;
    allocator->memory_end = memory_end;
    allocator->allocated_size = 0;
    allocator->remaining_size = memory_end - memory_start;
    return 0;
}

int bump_allocator_allocate(bump_allocator_t *allocator, uint64_t size, uint64_t *address) {
    if (allocator->remaining_size < size) {
        return -1;
    }
    *address = allocator->memory_start + allocator->allocated_size;
    allocator->allocated_size += size;
    allocator->remaining_size -= size;
    return 0;
}
