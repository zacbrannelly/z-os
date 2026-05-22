#pragma once

#include <stdint.h>
#include <libz/handle.h>

#include "../utils/vector.h"

// Forward declarations.
typedef struct file_t file_t;

typedef struct shared_memory_t {
    vector_t pages;
    uint64_t size;
} shared_memory_t;

int shared_memory_open(const char *path, uint64_t size, handle_t *fd);
int shared_memory_unlink(const char *path);

int shared_memory_read(file_t *file, void *buffer, uint64_t size);
int shared_memory_write(file_t *file, const void *buffer, uint64_t size);
int shared_memory_close(file_t *file);
