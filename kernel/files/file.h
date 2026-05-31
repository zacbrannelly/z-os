#pragma once

#include <stdint.h>
#include <libz/handle.h>

// Forward declarations.
typedef struct file_t file_t;

typedef struct file_ops_t {
    int (*open)(file_t *file, handle_t *handle);
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

/**
 * ------------ Library API for file ------------
 */

int file_open(const char *path, handle_t *fd);
int file_read(handle_t fd, void *buffer, uint64_t size);
int file_write(handle_t fd, const void *buffer, uint64_t size);
int file_close(handle_t fd);
