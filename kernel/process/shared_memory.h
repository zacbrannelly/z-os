#pragma once

#include <stdint.h>
#include <libz/handle.h>

#include "../utils/vector.h"

// Forward declarations.
typedef struct file_t file_t;

typedef struct shared_memory_t {
    vector_t pages;
    uint64_t size;
    uint8_t pages_owned;
} shared_memory_t;

/**
 * ------------ Kernel API for shared memory ------------
 */

// Create a shared memory object and store in global file table.
int shared_memory_create(
    const char *path,
    uint64_t size,
    shared_memory_t **shared_memory_ptr,
    handle_t *global_handle
);

// Create a shared memory object from an existing page range.
int shared_memory_create_from_contiguous_pages(
    const char *path,
    uint64_t physical_start_address,
    uint64_t size,
    shared_memory_t **shared_memory_ptr,
    handle_t *global_handle
);

// Destroy a shared memory object and remove from global file table.
int shared_memory_destroy(handle_t global_handle);

/**
 * ------------ Library API for shared memory ------------
 */

// Open/unlink a shared memory object and store in process file table.
int shared_memory_open(const char *path, uint64_t size, handle_t *fd);
int shared_memory_unlink(const char *path);

// Read/write/close a shared memory object.
int shared_memory_read(file_t *file, void *buffer, uint64_t size);
int shared_memory_write(file_t *file, const void *buffer, uint64_t size);
int shared_memory_close(file_t *file);
