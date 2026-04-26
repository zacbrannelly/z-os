#include "gfx.h"

#include <stddef.h>

#include "../kernel.h"
#include "../memory.h"
#include "../console.h"
#include "../page_alloc.h"
#include "../mmap.h"

// TODO: This just needs to come from the kernel heap space via kmalloc once it is implemented.
#define GFX_BACK_FRAMEBUFFER_VIRTUAL_BASE 0xffffffff90000000ULL

typedef struct gfx_t {
    uint32_t *framebuffer;       // Pointer to the framebuffer.
    uint32_t framebuffer_size;   // Size of the framebuffer in pixels.
    uint32_t framebuffer_width;  // Width of the framebuffer in pixels.
    uint32_t framebuffer_stride; // Stride of the framebuffer in bytes.

    uint64_t back_framebuffer_physical_address; // Physical address of the back framebuffer.
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

    if (page_alloc_block(PAGE_ALLOC_ORDER_4MB, &g_gfx.back_framebuffer_physical_address) < 0) {
        console_write("Failed to allocate back framebuffer\r\n");
        return -1;
    }
    
    if (mmap_map_range_l2_block(
        GFX_BACK_FRAMEBUFFER_VIRTUAL_BASE,
        GFX_BACK_FRAMEBUFFER_VIRTUAL_BASE + g_gfx.framebuffer_size,
        g_gfx.back_framebuffer_physical_address,
        PAGE_FLAG_NX | PAGE_FLAG_EL0_EL1_RW | PAGE_FLAG_NORMAL_MEMORY_NC
    ) < 0) {
        console_write("Failed to map back framebuffer\r\n");
        return -1;
    }

    g_gfx.back_framebuffer = (uint32_t *)GFX_BACK_FRAMEBUFFER_VIRTUAL_BASE;

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
