#include "compositor.h"

#include <stddef.h>
#include <libz/assert.h>
#include <libz/memory.h>
#include <libz/syscall.h>
#include <libz/malloc.h>
#include <libz/console.h>
#include <libz/string.h>
#include <libgfx/colors.h>
#include <libgfx/paint.h>
#include <libgfx/font.h>

#include "gfx.h"
#include "ui/cursor.h"
#include "ui/text_input.h"
#include "abi/compositor_abi.h"

#define WINDOW_TITLE_BAR_HEIGHT 28

typedef struct compositor_window_t {
    char *name;
    int32_t x;
    int32_t y;
    uint64_t width;
    uint64_t height;
    handle_t handle;
    linked_list_node_t *node;
} compositor_window_t;

static compositor_t g_compositor;

int compositor_init(void) {
    memory_set(&g_compositor, 0, sizeof(compositor_t));

    if (gfx_init() < 0) {
        console_write("compositor: gfx_init failed\r\n");
        return 1;
    }
    console_write("compositor: gfx_init succeeded\r\n");

    if (cursor_init(gfx_get_framebuffer_width(), gfx_get_framebuffer_height()) < 0) {
        console_write("compositor: cursor_init failed\r\n");
        return 1;
    }
    console_write("compositor: cursor_init succeeded\r\n");

    if (text_input_alloc(&g_compositor.text_input) < 0) {
        console_write("compositor: text_input_alloc failed\r\n");
        return 1;
    }
    console_write("compositor: text_input_alloc succeeded\r\n");

    // Initialize window table.
    if (handle_table_init(&g_compositor.window_table, 1024) < 0) {
        return -1;
    }

    // Initialize windows list.
    if (linked_list_init(&g_compositor.windows) < 0) {
        return -1;
    }

    if (compositor_abi_init(&g_compositor) < 0) {
        return -1;
    }
    console_write("compositor: compositor_abi_init succeeded\r\n");

    return 0;
}

static void compositor_render_window(compositor_window_t *window, bitmap_t *back_framebuffer) {
    // Draw title bar.
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

    // Draw window title.
    font_draw_text_bitmap(
        back_framebuffer,
        window->name,
        window->x + 8,
        window->y + 18,
        RGB_COLOR_WHITE
    );

    // Draw window content.
    paint_fill_rect(
        back_framebuffer,
        window->x,
        window->y + WINDOW_TITLE_BAR_HEIGHT,
        (uint32_t)window->width,
        (uint32_t)window->height,
        RGB_COLOR(25, 25, 25)
    );

    // Draw window border.
    paint_draw_rect(
        back_framebuffer,
        window->x,
        window->y + WINDOW_TITLE_BAR_HEIGHT - 1,
        (uint32_t)window->width,
        (uint32_t)window->height + 1,
        RGB_COLOR_WHITE
    );
}

int compositor_run(void) {
    bitmap_t *back_framebuffer = gfx_get_back_framebuffer();

    while (1) {
        // Poll for requests from other processes.
        compositor_abi_poll(&g_compositor);

        // Update cursor.
        cursor_update();

        // Clear screen.
        paint_clear(back_framebuffer, RGB_COLOR_BLACK);

        // Render text input.
        text_input_draw(&g_compositor.text_input);

        // Render windows.
        for (linked_list_node_t *node = g_compositor.windows.head; node != NULL; node = node->next) {
            compositor_window_t *window = (compositor_window_t *)node->data;
            compositor_render_window(window, back_framebuffer);
        }

        // Render cursor.
        cursor_draw();

        gfx_swap_buffers();
        syscall_yield();
    }

    return 0;
}

int compositor_window_create(
    const char *name,
    uint64_t width,
    uint64_t height,
    handle_t *window_handle
) {
    if (name == NULL || window_handle == NULL) {
        return -1;
    }

    compositor_window_t *window = malloc(sizeof(compositor_window_t));
    if (window == NULL) {
        return -1;
    }
    memory_set(window, 0, sizeof(compositor_window_t));
    window->x = 0;
    window->y = 0;
    window->name = (char *)malloc(strlen(name) + 1);
    memory_copy(window->name, (void *)name, strlen(name) + 1);
    window->width = width;
    window->height = height;

    // Register window with handle table.
    assert(handle_table_insert(&g_compositor.window_table, window, window_handle) == 0);
    window->handle = *window_handle;

    // Register window with windows list.
    assert(linked_list_insert(&g_compositor.windows, (void *)window, &window->node) == 0);

    return 0;
}
