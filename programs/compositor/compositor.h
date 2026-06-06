#pragma once

#include <libz/handle.h>
#include <libcommon/handle_table.h>
#include <libcommon/linked_list.h>
#include "ui/text_input.h"

typedef struct compositor_t {
    handle_t channel_fd;
    handle_table_t window_table;
    linked_list_t windows;

    // UI elements.
    text_input_t text_input;
} compositor_t;

int compositor_init(void);
int compositor_run(void);

int compositor_window_create(
    const char *name,
    uint64_t width,
    uint64_t height,
    handle_t *window_handle
);

int compositor_window_attach_buffer(
    handle_t window_handle,
    handle_t buffer_handle
);

int compositor_window_commit(
    handle_t window_handle,
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height
);
