#include "window_title_bar.h"

#include <stddef.h>
#include <libz/memory.h>
#include <libz/console.h>
#include <libgfx/colors.h>
#include <libgfx/paint.h>
#include <libgfx/font.h>

#include "../compositor.h"
#include "../gfx.h"
#include "cursor.h"

void window_title_bar_update(window_title_bar_t *window_title_bar) {
    int32_t cursor_x, cursor_y;
    cursor_get_position(&cursor_x, &cursor_y);

    // Check if the cursor is inside the window title bar.
    compositor_window_t *window = window_title_bar->window;
    if (cursor_is_in_rect(window->x, window->y, window->width, WINDOW_TITLE_BAR_HEIGHT)) {
        if (cursor_is_left_click() && !window_title_bar->dragging) {
            // Enable dragging.
            window_title_bar->dragging = 1;
            window_title_bar->drag_start_x = window->x;
            window_title_bar->drag_start_y = window->y;
            window_title_bar->drag_offset_x = cursor_x;
            window_title_bar->drag_offset_y = cursor_y;
        }
    }

    // Drag the window if it is being dragged.
    if (window_title_bar->dragging && cursor_is_left_click()) {
        window->x = window_title_bar->drag_start_x + (cursor_x - window_title_bar->drag_offset_x);
        window->y = window_title_bar->drag_start_y + (cursor_y - window_title_bar->drag_offset_y);
    } else if (window_title_bar->dragging) {
        // Disable dragging.
        window_title_bar->dragging = 0;
    }
}

void window_title_bar_render(window_title_bar_t *window_title_bar, bitmap_t *back_framebuffer) {
    compositor_window_t *window = window_title_bar->window;

    // Draw the background.
    paint_fill_rect(
        back_framebuffer,
        window->x,
        window->y,
        (uint32_t)window->width,
        WINDOW_TITLE_BAR_HEIGHT,
        RGB_COLOR_BLACK
    );
    paint_draw_rect(
        back_framebuffer,
        window->x,
        window->y,
        (uint32_t)window->width,
        WINDOW_TITLE_BAR_HEIGHT,
        RGB_COLOR_WHITE
    );

    // Draw title text.
    font_draw_text_bitmap(
        back_framebuffer,
        window->name,
        window->x + 8,
        window->y + 18,
        RGB_COLOR_WHITE
    );
}
