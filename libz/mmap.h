#pragma once

#include <stdint.h>
#include <libz/handle.h>

// Flags for the mmap system call.
#define MAP_READ      (1 << 0)
#define MAP_WRITE     (1 << 1)
#define MAP_PRIVATE   (1 << 2)
#define MAP_ANONYMOUS (1 << 3)
#define MAP_SHARED    (1 << 4)

#define MAP_PAGE_SIZE 4096

// Map memory.
uint64_t mmap(uint64_t address, uint64_t length, uint64_t flags, handle_t fd);
void munmap(void *address, uint64_t length);
