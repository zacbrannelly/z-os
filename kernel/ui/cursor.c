#include "cursor.h"

#include <stddef.h>

#include "../console.h"
#include "../math.h"
#include "../gfx/gfx.h"
#include "../drivers/usb/usb_hid_mouse.h"

#define CURSOR_WIDTH 10
#define CURSOR_HEIGHT 10

#define CURSOR_IDLE_COLOR        GFX_COLOR(0, 0, 0)
#define CURSOR_LEFT_CLICK_COLOR  GFX_COLOR(255, 0, 0)
#define CURSOR_RIGHT_CLICK_COLOR GFX_COLOR(0, 255, 0)

typedef struct cursor_t {
    int32_t x;
    int32_t y;
    uint32_t bounds_x;
    uint32_t bounds_y;
    uint8_t last_report_cycle_bit;

    uint8_t left_click;
    uint8_t right_click;
} cursor_t;

static cursor_t g_cursor;

int cursor_init(uint32_t bounds_x, uint32_t bounds_y) {
    g_cursor.x = 0;
    g_cursor.y = 0;
    g_cursor.bounds_x = bounds_x;
    g_cursor.bounds_y = bounds_y;
    g_cursor.left_click = 0;
    g_cursor.right_click = 0;
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
    usb_hid_mouse_report_t *report = usb_hid_mouse_get_report();
    if (report == NULL) {
        return;
    }

    if (report->cycle_bit == g_cursor.last_report_cycle_bit) {
        return;
    }

    cursor_move(report->x, report->y);

    if (report->buttons & USB_HID_MOUSE_BUTTON_LEFT) {
        g_cursor.left_click = 1;
    } else {
        g_cursor.left_click = 0;
    }
    if (report->buttons & USB_HID_MOUSE_BUTTON_RIGHT) {
        g_cursor.right_click = 1;
    } else {
        g_cursor.right_click = 0;
    }

    g_cursor.last_report_cycle_bit = report->cycle_bit;
}

void cursor_draw(void) {
    uint32_t color = CURSOR_IDLE_COLOR;
    if (g_cursor.left_click) {
        color = CURSOR_LEFT_CLICK_COLOR;
    } else if (g_cursor.right_click) {
        color = CURSOR_RIGHT_CLICK_COLOR;
    }
    gfx_fill_rect(g_cursor.x, g_cursor.y, CURSOR_WIDTH, CURSOR_HEIGHT, color);
}
