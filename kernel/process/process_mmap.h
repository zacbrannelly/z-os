#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct process_t process_t;
typedef struct exception_frame_t exception_frame_t;

// Flags for the mmap system call.
#define MAP_READ      0x1
#define MAP_WRITE     0x2
#define MAP_PRIVATE   0x4
#define MAP_ANONYMOUS 0x8

// Map process memory.
uint64_t process_mmap(
    process_t *process,
    uint64_t address,
    uint64_t length,
    uint64_t flags
);

// Unmap process memory.
void process_munmap(
    process_t *process,
    void *address, 
    uint64_t length
);
