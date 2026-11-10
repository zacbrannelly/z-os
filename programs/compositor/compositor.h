#pragma once

#include <libz/handle.h>
#include <libcommon/handle_table.h>
#include <libcommon/linked_list.h>
#include <libgfx/bitmap.h>
#include "ui/text_input.h"
#include "ui/window_title_bar.h"

typedef struct compositor_window_t {
    char *name;
    int32_t x;
    int32_t y;
    uint64_t width;
    uint64_t height;
    handle_t handle;

    // UI elements.
    window_title_bar_t title_bar;

    // Shared memory.
    handle_t buffer_handle;
    bitmap_t *buffer;
    linked_list_node_t *node;

    // Framebuffer.
    bitmap_t *framebuffer;
} compositor_window_t;

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
