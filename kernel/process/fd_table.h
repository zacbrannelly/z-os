#pragma once

#include "../utils/handle_table.h"

#include <stdint.h>

// Forward declarations.
typedef struct file_t file_t;

typedef struct fd_table_t {
    handle_table_t handles;
} fd_table_t;

int fd_table_init(fd_table_t *fd_table);
int fd_table_destroy(fd_table_t *fd_table);

int fd_table_open(fd_table_t *fd_table, file_t *file, handle_t *fd);
int fd_table_close(fd_table_t *fd_table, handle_t fd);

int fd_table_get(fd_table_t *fd_table, handle_t fd, file_t **file);
