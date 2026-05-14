#pragma once

#include <stdint.h>

// Flags for the mmap system call.
#define MAP_READ      0x1
#define MAP_WRITE     0x2
#define MAP_PRIVATE   0x4
#define MAP_ANONYMOUS 0x8

#define MAP_PAGE_SIZE 4096

// Map memory.
uint64_t mmap(uint64_t address, uint64_t length, uint64_t flags);
void munmap(void *address, uint64_t length);
