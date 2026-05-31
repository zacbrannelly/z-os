#include "gfx.h"

#include <stddef.h>
#include <libgfx/paint.h>

typedef struct gfx_t {
    bitmap_t *framebuffer;
    bitmap_t *back_framebuffer;
} gfx_t;

static gfx_t g_gfx;

int gfx_init(uint32_t *framebuffer, uint32_t width, uint32_t height) {
    g_gfx.framebuffer = bitmap_from_data((uint8_t *)framebuffer, width, height, BITMAP_PIXEL_FORMAT_RGB32);
    if (g_gfx.framebuffer == NULL) {
        return -1;
    }

    g_gfx.back_framebuffer = bitmap_create(width, height, BITMAP_PIXEL_FORMAT_RGB32);
    if (g_gfx.back_framebuffer == NULL) {
        return -1;
    }

    return 0;
}

bitmap_t *gfx_get_back_framebuffer(void) {
    return g_gfx.back_framebuffer;
}

uint32_t gfx_get_framebuffer_width(void) {
    return g_gfx.framebuffer->width;
}

uint32_t gfx_get_framebuffer_height(void) {
    return g_gfx.framebuffer->height;
}

void gfx_swap_buffers(void) {
    paint_swap_buffers(g_gfx.framebuffer, g_gfx.back_framebuffer);
}
