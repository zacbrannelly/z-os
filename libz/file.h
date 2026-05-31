#pragma once

#include <stdint.h>
#include <libz/handle.h>

int open(const char *path, handle_t *fd);
int read(handle_t fd, void *buffer, uint64_t size);
int write(handle_t fd, const void *buffer, uint64_t size);
int close(handle_t fd);
