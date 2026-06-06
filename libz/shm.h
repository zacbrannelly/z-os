#pragma once

#include <stdint.h>
#include <libz/handle.h>

int shm_open(const char *path, uint64_t size, handle_t *fd);
int shm_unlink(const char *path);
