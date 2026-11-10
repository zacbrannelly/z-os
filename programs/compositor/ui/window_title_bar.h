#pragma once

#include <stdint.h>
#include <libz/handle.h>

#define WINDOW_TITLE_BAR_HEIGHT 28

// Forward declarations.
typedef struct compositor_window_t compositor_window_t;
typedef struct bitmap_t bitmap_t;

typedef struct window_title_bar_t {
    compositor_window_t *window;
    uint8_t dragging;
    int32_t drag_start_x;
    int32_t drag_start_y;
    int32_t drag_offset_x;
    int32_t drag_offset_y;
} window_title_bar_t;

void window_title_bar_update(window_title_bar_t *window_title_bar);
void window_title_bar_render(window_title_bar_t *window_title_bar, bitmap_t *back_framebuffer);
