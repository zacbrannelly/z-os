#pragma once

#include <stdint.h>
#include <libgfx/bitmap.h>

// TODO: Expose these from the kernel.
#define FRAMEBUFFER_WIDTH 800
#define FRAMEBUFFER_HEIGHT 600
#define FRAMEBUFFER_SIZE (FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT * sizeof(uint32_t))

int gfx_init(void);

bitmap_t *gfx_get_back_framebuffer(void);
uint32_t gfx_get_framebuffer_width(void);
uint32_t gfx_get_framebuffer_height(void);

void gfx_swap_buffers(void);
