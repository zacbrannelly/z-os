#pragma once

#include <stdint.h>
#include <libz/handle.h>

int window_create(
    const char *name,
    uint64_t width,
    uint64_t height,
    handle_t *window_handle
);
