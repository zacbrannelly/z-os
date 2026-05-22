#pragma once

#include <stdint.h>
#include <libz/handle.h>

uint64_t shm_open(const char *path, uint64_t size, handle_t *fd);
uint64_t shm_unlink(const char *path);
