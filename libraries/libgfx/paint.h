#pragma once

#include <stdint.h>

#include "bitmap.h"

void paint_clear(bitmap_t *bitmap, uint32_t color);

void paint_fill_rect(
    bitmap_t *bitmap,
    int32_t x,
    int32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t color
);

void paint_draw_rect(
    bitmap_t *bitmap,
    int32_t x,
    int32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t color
);

void paint_draw_alpha_bitmap_scaled(
    bitmap_t *bitmap,
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

void paint_swap_buffers(bitmap_t *bitmap, bitmap_t *back_bitmap);
