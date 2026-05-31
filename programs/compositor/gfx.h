#pragma once

#include <stdint.h>
#include <libgfx/bitmap.h>

int gfx_init(uint32_t *framebuffer, uint32_t width, uint32_t height);

bitmap_t *gfx_get_back_framebuffer(void);
uint32_t gfx_get_framebuffer_width(void);
uint32_t gfx_get_framebuffer_height(void);

void gfx_swap_buffers(void);
