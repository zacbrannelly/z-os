#pragma once

#include <stdint.h>

int kernel_heap_init(void);

// Allocate memory from the kernel heap.
void *kmalloc(uint64_t size);

// Free memory from the kernel heap.
void kfree(void *address);
