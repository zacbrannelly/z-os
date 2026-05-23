#pragma once

#include <stdint.h>
#include <libz/handle.h>

int channel_open(const char *path, handle_t *fd);
int channel_close(handle_t fd);

int channel_send(handle_t fd, const void *data, uint64_t size);
int channel_recv(handle_t fd, void *data, uint64_t size);
