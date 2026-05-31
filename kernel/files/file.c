#include "file.h"

#include <stddef.h>

#include "../assert.h"
#include "../console.h"
#include "../scheduler/thread.h"
#include "../scheduler/scheduler.h"
#include "../process/process.h"
#include "../process/fd_table.h"
#include "file_table.h"

/**
 * ------------ Library API for file ------------
 */

int file_open(const char *path, handle_t *fd) {
    if (path == NULL || fd == NULL) {
        return -1;
    }

    // For open operations, we get the file object from the global file table instead.
    file_t *file = NULL;
    if (file_table_get_by_path(path, &file) < 0) {
        console_write("file: ");
        console_write(path);
        console_write(" not found\r\n");
        return -1;
    }

    if (file->ops.open == NULL) {
        return -1;
    }

    return file->ops.open(file, fd);
}

int file_read(handle_t fd, void *buffer, uint64_t size) {
    if (fd < 0 || buffer == NULL || size == 0) {
        return -1;
    }

    thread_t *thread = scheduler_get_current_thread();
    assert(thread != NULL);
    assert(thread->process != NULL);

    fd_table_t *fd_table = &thread->process->fd_table;
    file_t *file = NULL;
    if (fd_table_get(fd_table, fd, &file) < 0) {
        return -1;
    }

    assert(file != NULL);
    assert(file->ops.read != NULL);

    return file->ops.read(file, buffer, size);
}

int file_write(handle_t fd, const void *buffer, uint64_t size) {
    if (fd < 0 || buffer == NULL || size == 0) {
        return -1;
    }

    thread_t *thread = scheduler_get_current_thread();
    assert(thread != NULL);
    assert(thread->process != NULL);

    fd_table_t *fd_table = &thread->process->fd_table;
    file_t *file = NULL;
    if (fd_table_get(fd_table, fd, &file) < 0) {
        return -1;
    }

    assert(file != NULL);
    assert(file->ops.write != NULL);

    return file->ops.write(file, buffer, size);
}

int file_close(handle_t fd) {
    if (fd < 0) {
        return -1;
    }

    thread_t *thread = scheduler_get_current_thread();
    assert(thread != NULL);
    assert(thread->process != NULL);

    fd_table_t *fd_table = &thread->process->fd_table;
    file_t *file = NULL;
    if (fd_table_get(fd_table, fd, &file) < 0) {
        return -1;
    }

    assert(file != NULL);
    assert(file->ops.close != NULL);

    return file->ops.close(file);
}
