#include "gfx.h"

#include <stddef.h>

#include "../kernel.h"
#include "../memory.h"
#include "../console.h"
#include "../kmalloc.h"

#include "font.h"

typedef struct gfx_t {
    uint32_t *framebuffer;       // Pointer to the framebuffer.
    uint32_t framebuffer_size;   // Size of the framebuffer in pixels.
    uint32_t framebuffer_width;  // Width of the framebuffer in pixels.
    uint32_t framebuffer_stride; // Stride of the framebuffer in bytes.

    uint32_t *back_framebuffer;  // Pointer to the back framebuffer.
} gfx_t;

static gfx_t g_gfx;

int gfx_init(boot_info_t *boot_info) {
    if (boot_info == NULL) {
        console_write("Failed to initialize graphics: boot info is NULL\r\n");
        return -1;
    }

    g_gfx.framebuffer = boot_info->framebuffer;
    g_gfx.framebuffer_size = boot_info->framebuffer_size;
    g_gfx.framebuffer_width = boot_info->framebuffer_width;
    g_gfx.framebuffer_stride = boot_info->framebuffer_stride;

    g_gfx.back_framebuffer = (uint32_t *)kmalloc(g_gfx.framebuffer_size * sizeof(uint32_t));
    if (g_gfx.back_framebuffer == NULL) {
        console_write("Failed to allocate back framebuffer\r\n");
        return -1;
    }
    memory_set(g_gfx.back_framebuffer, 0, g_gfx.framebuffer_size * sizeof(uint32_t));

    if (font_init() < 0) {
        console_write("Failed to initialize font system\r\n");
        return -1;
    }

    return 0;
}

void gfx_clear(uint32_t color) {
    for (uint32_t i = 0; i < g_gfx.framebuffer_size; i++) {
        g_gfx.back_framebuffer[i] = color;
    }
}

void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++) {
            g_gfx.back_framebuffer[x + j + (y + i) * g_gfx.framebuffer_width] = color;
        }
    }
}

void gfx_draw_alpha_bitmap(
    uint8_t *src_bitmap,
    uint32_t src_stride,
    uint32_t src_x,
    uint32_t src_y,
    uint32_t src_width,
    uint32_t src_height,
    uint32_t dst_x,
    uint32_t dst_y,
    uint32_t color
) {
    uint32_t framebuffer_width = gfx_get_framebuffer_width();
    uint32_t framebuffer_height = gfx_get_framebuffer_height();

    for (uint32_t i = 0; i < src_height; i++) {
        for (uint32_t j = 0; j < src_width; j++) {
            int dst_x0 = dst_x + j;
            int dst_y0 = dst_y + i;

            if (dst_x0 < 0 || dst_y0 < 0 || dst_x0 >= framebuffer_width || dst_y0 >= framebuffer_height)
                continue;

            uint8_t pixel = src_bitmap[src_x + j + (src_y + i) * src_stride];
            if (pixel == 0) continue;

            uint32_t *pixel_ptr = &g_gfx.back_framebuffer[dst_x + j + (dst_y + i) * g_gfx.framebuffer_width];
            uint32_t existing_color = *pixel_ptr;

            uint32_t inv_alpha = 255 - pixel;

            uint32_t src_red = (color >> 16) & 0xFF;
            uint32_t src_green = (color >> 8) & 0xFF;
            uint32_t src_blue = color & 0xFF;

            uint32_t dst_red = (existing_color >> 16) & 0xFF;
            uint32_t dst_green = (existing_color >> 8) & 0xFF;
            uint32_t dst_blue = existing_color & 0xFF;

            uint32_t red = (src_red * pixel + dst_red * inv_alpha) / 255;
            uint32_t green = (src_green * pixel + dst_green * inv_alpha) / 255;
            uint32_t blue = (src_blue * pixel + dst_blue * inv_alpha) / 255;

            *pixel_ptr = (red << 16) | (green << 8) | blue;
        }
    }
}

void gfx_swap_buffers(void) {
    for (uint32_t i = 0; i < g_gfx.framebuffer_size; i++) {
        g_gfx.framebuffer[i] = g_gfx.back_framebuffer[i];
    }
}

uint32_t gfx_get_framebuffer_width(void) {
    return g_gfx.framebuffer_width;
}

uint32_t gfx_get_framebuffer_height(void) {
    return (g_gfx.framebuffer_size * sizeof(uint32_t)) / g_gfx.framebuffer_stride;
}
