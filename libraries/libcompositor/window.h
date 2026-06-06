#pragma once

#include <stdint.h>
#include <libz/handle.h>

int window_create(
    const char *name,
    uint64_t width,
    uint64_t height,
    handle_t *window_handle
);

int window_attach_buffer(
    handle_t window_handle,
    handle_t buffer_handle
);

int window_commit(
    handle_t window_handle,
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height
);
