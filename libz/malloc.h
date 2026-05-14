#pragma once

#include <stdint.h>

void malloc_init(void);

// Allocate memory.
void *malloc(uint64_t size);

// Free memory.
void free(void *ptr);
