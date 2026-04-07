#pragma once

#include <stdint.h>

typedef struct bump_allocator_t {
    uint64_t memory_start;
    uint64_t memory_end;
    uint64_t allocated_size;
    uint64_t remaining_size;
} bump_allocator_t;

int bump_allocator_make_invalid(bump_allocator_t *allocator);
int bump_allocator_invalid(bump_allocator_t *allocator);
int bump_allocator_init(bump_allocator_t *allocator, uint64_t memory_start, uint64_t memory_end);
int bump_allocator_allocate(bump_allocator_t *allocator, uint64_t size, uint64_t *address);
