#pragma once

#include <stdint.h>
#include <libz/handle.h>
#include <libz/mmap.h>

// Forward declarations.
typedef struct process_t process_t;
typedef struct exception_frame_t exception_frame_t;

// Map process memory.
uint64_t process_mmap(
    process_t *process,
    uint64_t address,
    uint64_t length,
    uint64_t flags,
    handle_t fd
);

// Unmap process memory.
void process_munmap(
    process_t *process,
    void *address, 
    uint64_t length
);
