#pragma once

#include <stdint.h>
#include "colors.h"

// Forward declarations.
typedef struct boot_info_t boot_info_t;
typedef struct bitmap_t bitmap_t;

int gfx_init(boot_info_t *boot_info);
void gfx_clear(uint32_t color);
void gfx_fill_rect(int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color);
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
);
void gfx_swap_buffers(void);

bitmap_t *gfx_get_framebuffer(void);
bitmap_t *gfx_get_back_framebuffer(void);

uint32_t gfx_get_framebuffer_width(void);
uint32_t gfx_get_framebuffer_height(void);
