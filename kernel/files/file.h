#pragma once

#include <stdint.h>
#include <libz/handle.h>

// Forward declarations.
typedef struct file_t file_t;

typedef struct file_ops_t {
    int (*read)(file_t *file, void *buffer, uint64_t size);
    int (*write)(file_t *file, const void *buffer, uint64_t size);
    int (*close)(file_t *file);
} file_ops_t;

typedef struct file_t {
    handle_t handle;
    char *path;
    uint64_t ref_count;
    void *private_data;
    file_ops_t ops;
} file_t;
