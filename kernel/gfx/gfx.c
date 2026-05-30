#include "gfx.h"

#include <stddef.h>
#include <libz/handle.h>

#include <libgfx/colors.h>
#include <libgfx/bitmap.h>
#include <libgfx/paint.h>
#include <libgfx/font.h>

#include "../assert.h"
#include "../kernel.h"
#include "../memory.h"
#include "../console.h"
#include "../kmalloc.h"
#include "../mmap.h"
#include "../process/shared_memory.h"

#define GFX_VIRTUAL_BASE_ADDRESS 0xFFFF400000000000ULL
#define GFX_PAGE_FLAGS PAGE_FLAG_EL1_RW | PAGE_FLAG_NX | PAGE_FLAG_NORMAL_MEMORY_NC | PAGE_FLAG_INNER_SHARABLE | PAGE_FLAG_ACCESS

typedef struct gfx_t {
    bitmap_t *framebuffer;
    bitmap_t *back_framebuffer;
} gfx_t;

static gfx_t g_gfx;

int gfx_init(boot_info_t *boot_info) {
    if (boot_info == NULL) {
        console_write("Failed to initialize graphics: boot info is NULL\r\n");
        return -1;
    }

    uint32_t width = boot_info->framebuffer_width;
    uint32_t height = boot_info->framebuffer_size / width;
    g_gfx.framebuffer = bitmap_from_data(
        (uint8_t *)GFX_VIRTUAL_BASE_ADDRESS,
        width,
        height,
        BITMAP_PIXEL_FORMAT_RGB32
    );

    if (g_gfx.framebuffer == NULL) {
        console_write("Failed to create framebuffer\r\n");
        return -1;
    }

    // Map the physical framebuffer to the virtual address space.
    if (mmap_map_range(
        GFX_VIRTUAL_BASE_ADDRESS,
        GFX_VIRTUAL_BASE_ADDRESS + boot_info->framebuffer_size * sizeof(uint32_t),
        (uint64_t)boot_info->framebuffer,
        GFX_PAGE_FLAGS
    ) < 0) {
        console_write("Failed to map framebuffer\r\n");
        return -1;
    }

    // Map the physical framebuffere to /dev/fb0 (for userland access).
    shared_memory_t *shared_memory = NULL;
    handle_t global_handle = 0;
    assert(shared_memory_create_from_contiguous_pages(
        "/dev/fb0",
        (uint64_t)boot_info->framebuffer,
        boot_info->framebuffer_size * sizeof(uint32_t),
        &shared_memory,
        &global_handle
    ) == 0);
    assert(shared_memory != NULL);
    assert(global_handle >= 0);

    // Create the back framebuffer.
    g_gfx.back_framebuffer = bitmap_create(
        width,
        height,
        BITMAP_PIXEL_FORMAT_RGB32
    );
    if (g_gfx.back_framebuffer == NULL) {
        console_write("Failed to create back framebuffer\r\n");
        return -1;
    }

    if (font_init() < 0) {
        console_write("Failed to initialize font system\r\n");
        return -1;
    }

    return 0;
}

void gfx_clear(uint32_t color) {
    paint_clear(g_gfx.back_framebuffer, color);
}

void gfx_fill_rect(int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color) {
    paint_fill_rect(g_gfx.back_framebuffer, x, y, width, height, color);
}

void gfx_draw_alpha_bitmap_scaled(
    uint8_t *src_bitmap,
    uint32_t src_stride,
    uint32_t src_x,
    uint32_t src_y,
    uint32_t src_width,
    uint32_t src_height,
    uint32_t dst_x,
    uint32_t dst_y,
    uint32_t dst_width,
    uint32_t dst_height,
    uint32_t color
) {
    paint_draw_alpha_bitmap_scaled(
        g_gfx.back_framebuffer,
        src_bitmap,
        src_stride,
        src_x,
        src_y,
        src_width,
        src_height,
        dst_x,
        dst_y,
        dst_width,
        dst_height,
        color
    );
}

void gfx_swap_buffers(void) {
    paint_swap_buffers(g_gfx.framebuffer, g_gfx.back_framebuffer);
}

bitmap_t *gfx_get_framebuffer(void) {
    return g_gfx.framebuffer;
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

void *gfx_alloc(uint64_t size) {
    return kmalloc(size);
}

void gfx_free(void *address) {
    kfree(address);
}
