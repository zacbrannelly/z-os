#pragma once

#include <stdint.h>
#include <libz/handle.h>

// Forward declarations.
typedef struct file_t file_t;

int channel_open(const char *path, handle_t *fd, int flags);
int channel_close(handle_t fd);

int channel_send(handle_t fd, const void *data, uint64_t size);
int channel_recv(handle_t fd, void *data, uint64_t size);

int channel_send_fd(handle_t channel_fd, handle_t fd, const void *data, uint64_t size);
int channel_recv_fd(handle_t channel_fd, handle_t* fd, void *data, uint64_t size);

int channel_file_read(file_t *file, void *data, uint64_t size);
int channel_file_write(file_t *file, const void *data, uint64_t size);
int channel_file_close(file_t *file);
