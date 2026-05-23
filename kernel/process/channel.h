#pragma once

#include <stdint.h>
#include <libz/handle.h>
#include "../utils/linked_list.h"

// Forward declarations.
typedef struct file_t file_t;

typedef struct channel_t {
    linked_list_t messages;
    linked_list_t recv_waiters;
} channel_t;

int channel_open(const char *path, handle_t *fd);
int channel_close(handle_t fd);

int channel_send(handle_t fd, const void *data, uint64_t size);
int channel_recv(handle_t fd, void *data, uint64_t size);

int channel_file_read(file_t *file, void *data, uint64_t size);
int channel_file_write(file_t *file, const void *data, uint64_t size);
int channel_file_close(file_t *file);
