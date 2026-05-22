#include "fd_table.h"

#include <stddef.h>

#define DEFAULT_FD_TABLE_CAPACITY 64

int fd_table_init(fd_table_t *fd_table) {
    if (fd_table == NULL) {
        return -1;
    }

    if (handle_table_init(&fd_table->handles, DEFAULT_FD_TABLE_CAPACITY) < 0) {
        return -1;
    }

    return 0;
}

int fd_table_destroy(fd_table_t *fd_table) {
    if (fd_table == NULL) {
        return -1;
    }

    if (handle_table_destroy(&fd_table->handles) < 0) {
        return -1;
    }

    return 0;
}

int fd_table_open(fd_table_t *fd_table, file_t *file, handle_t *fd) {
    if (fd_table == NULL || file == NULL || fd == NULL) {
        return -1;
    }

    if (handle_table_insert(&fd_table->handles, file, fd) < 0) {
        return -1;
    }

    return 0;
}

int fd_table_close(fd_table_t *fd_table, handle_t fd) {
    if (fd_table == NULL || fd < 0) {
        return -1;
    }

    if (handle_table_remove(&fd_table->handles, fd) < 0) {
        return -1;
    }

    return 0;
}

int fd_table_get(fd_table_t *fd_table, handle_t fd, file_t **file) {
    if (fd_table == NULL || file == NULL || fd < 0) {
        return -1;
    }

    return handle_table_get(&fd_table->handles, fd, (void **)file);
}
