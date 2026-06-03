#include "gfx.h"

#include <stddef.h>
#include <libgfx/paint.h>
#include <libgfx/font.h>
#include <libz/console.h>
#include <libz/shm.h>
#include <libz/mmap.h>
#include <libz/memory.h>

typedef struct gfx_t {
    handle_t framebuffer_fd;
    bitmap_t *framebuffer;
    bitmap_t *back_framebuffer;
} gfx_t;

static gfx_t g_gfx;

int gfx_init(void) {
    memory_set(&g_gfx, 0, sizeof(gfx_t));

    // Open the shared memory object for the framebuffer.
    if (shm_open("/dev/fb0", FRAMEBUFFER_SIZE, &g_gfx.framebuffer_fd) != 0) {
        console_write("compositor: shm_open failed\r\n");
        return 1;
    }
    console_write("compositor: shm_open succeeded\r\n");

    // Map the framebuffer to the process's address space.
    uint32_t *framebuffer = (uint32_t *)mmap(0, FRAMEBUFFER_SIZE, MAP_SHARED | MAP_READ | MAP_WRITE, g_gfx.framebuffer_fd);
    if (framebuffer == NULL) {
        console_write("compositor: mmap failed\r\n");
        return 1;
    }
    console_write("compositor: framebuffer mmap succeeded\r\n");

    g_gfx.framebuffer = bitmap_from_data((uint8_t *)framebuffer, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, BITMAP_PIXEL_FORMAT_RGB32);
    if (g_gfx.framebuffer == NULL) {
        return -1;
    }

    g_gfx.back_framebuffer = bitmap_create(FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, BITMAP_PIXEL_FORMAT_RGB32);
    if (g_gfx.back_framebuffer == NULL) {
        return -1;
    }

    // Initialize the font system.
    if (font_init() < 0) {
        console_write("compositor: font_init failed\r\n");
        return -1;
    }
    console_write("compositor: font_init succeeded\r\n");

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
