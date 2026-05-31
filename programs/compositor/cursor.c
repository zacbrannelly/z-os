#include "cursor.h"

#include <stddef.h>
#include <libz/math.h>
#include <libz/console.h>
#include <libgfx/colors.h>
#include <libgfx/paint.h>
#include <libinput/input.h>

#include "gfx.h"

#define CURSOR_WIDTH 10
#define CURSOR_HEIGHT 10

#define CURSOR_IDLE_COLOR        RGB_COLOR(255, 255, 255)
#define CURSOR_LEFT_CLICK_COLOR  RGB_COLOR(255, 0, 0)
#define CURSOR_RIGHT_CLICK_COLOR RGB_COLOR(0, 255, 0)

typedef struct cursor_t {
    int32_t x;
    int32_t y;
    uint32_t bounds_x;
    uint32_t bounds_y;

    uint8_t left_click;
    uint8_t right_click;

    handle_t mouse_input_fd;
} cursor_t;

static cursor_t g_cursor;

int cursor_init(uint32_t bounds_x, uint32_t bounds_y) {
    g_cursor.x = 0;
    g_cursor.y = 0;
    g_cursor.bounds_x = bounds_x;
    g_cursor.bounds_y = bounds_y;
    g_cursor.left_click = 0;
    g_cursor.right_click = 0;

    // Open the mouse input file.
    if (input_open_nonblock("/dev/mouse0", &g_cursor.mouse_input_fd) < 0) {
        return -1;
    }

    return 0;
}

void cursor_set_position(int32_t x, int32_t y) {
    g_cursor.x = math_clamp_int32(x, 0, g_cursor.bounds_x - CURSOR_WIDTH);
    g_cursor.y = math_clamp_int32(y, 0, g_cursor.bounds_y - CURSOR_HEIGHT);
}

void cursor_move(int8_t dx, int8_t dy) {
    if (dx == 0 && dy == 0) {
        return;
    }
    cursor_set_position(g_cursor.x + dx, g_cursor.y + dy);
}

void cursor_get_position(int32_t *x, int32_t *y) {
    *x = g_cursor.x;
    *y = g_cursor.y;
}

void curor_set_bounds(uint32_t bounds_x, uint32_t bounds_y) {
    g_cursor.bounds_x = bounds_x;
    g_cursor.bounds_y = bounds_y;
}

void cursor_get_bounds(uint32_t *bounds_x, uint32_t *bounds_y) {
    *bounds_x = g_cursor.bounds_x;
    *bounds_y = g_cursor.bounds_y;
}

void cursor_update(void) {
    input_device_event_t event;
    if (input_read(g_cursor.mouse_input_fd, &event) < 0) return;

    // Handle the mouse move event.
    if (event.type == INPUT_DEVICE_EVENT_TYPE_MOUSE_MOVE_EVENT) {
        cursor_move(event.mouse_move_event.delta_x, event.mouse_move_event.delta_y);
    }

    // Handle the mouse button up event.
    if (event.type == INPUT_DEVICE_EVENT_TYPE_MOUSE_UP_EVENT) {
        if (event.mouse_button_event.button == INPUT_DEVICE_MOUSE_BUTTON_LEFT) {
            g_cursor.left_click = 0;
        } else if (event.mouse_button_event.button == INPUT_DEVICE_MOUSE_BUTTON_RIGHT) {
            g_cursor.right_click = 0;
        }
    }

    // Handle the mouse button down event.
    if (event.type == INPUT_DEVICE_EVENT_TYPE_MOUSE_DOWN_EVENT) {
        if (event.mouse_button_event.button == INPUT_DEVICE_MOUSE_BUTTON_LEFT) {
            g_cursor.left_click = 1;
        } else if (event.mouse_button_event.button == INPUT_DEVICE_MOUSE_BUTTON_RIGHT) {
            g_cursor.right_click = 1;
        }
    }
}

void cursor_draw(void) {
    uint32_t color = CURSOR_IDLE_COLOR;
    if (g_cursor.left_click) {
        color = CURSOR_LEFT_CLICK_COLOR;
    } else if (g_cursor.right_click) {
        color = CURSOR_RIGHT_CLICK_COLOR;
    }
    paint_fill_rect(gfx_get_back_framebuffer(), g_cursor.x, g_cursor.y, CURSOR_WIDTH, CURSOR_HEIGHT, color);
}
