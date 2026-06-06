#include "paint.h"

#include <libz/assert.h>
#include <stddef.h>

void paint_clear(bitmap_t *bitmap, uint32_t color) {
    uint32_t pixel_size = bitmap_pixel_format_size(bitmap->pixel_format);

    for (uint32_t y = 0; y < bitmap->height; y++) {
        for (uint32_t x = 0; x < bitmap->stride; x += pixel_size) {
            uint32_t *pixel_ptr = (uint32_t *)&bitmap->data[x + y * bitmap->stride];
            *pixel_ptr = color;
        }
    }
}

void paint_fill_rect(bitmap_t *bitmap, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color) {
    uint32_t pixel_size = bitmap_pixel_format_size(bitmap->pixel_format);

    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++) {
            int32_t x0 = x + j;
            int32_t y0 = y + i;
            if (x0 < 0 || y0 < 0 || x0 >= bitmap->width || y0 >= bitmap->height)
                continue;

            uint32_t *pixel_ptr = (uint32_t *)&bitmap->data[x0 * pixel_size + y0 * bitmap->stride];
            *pixel_ptr = color;
        }
    }
}

void paint_draw_rect(bitmap_t *bitmap, int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t color) {
    uint32_t pixel_size = bitmap_pixel_format_size(bitmap->pixel_format);

    // Draw top and bottom edges.
    paint_fill_rect(bitmap, x, y, width, 1, color);
    paint_fill_rect(bitmap, x, y + height - 1, width, 1, color);

    // Draw left and right edges.
    paint_fill_rect(bitmap, x, y, 1, height, color);
    paint_fill_rect(bitmap, x + width - 1, y, 1, height, color);
}

void paint_blit_bitmap(bitmap_t *dst, bitmap_t *src, int32_t x, int32_t y) {
    uint32_t pixel_size = bitmap_pixel_format_size(dst->pixel_format);

    for (uint32_t i = 0; i < src->height; i++) {
        for (uint32_t j = 0; j < src->width; j++) {
            int32_t x0 = x + j;
            int32_t y0 = y + i;

            if (x0 < 0 || y0 < 0 || x0 >= dst->width || y0 >= dst->height)
                continue;

            uint32_t *dst_pixel_ptr = (uint32_t *)&dst->data[x0 * pixel_size + y0 * dst->stride];
            uint32_t *src_pixel_ptr = (uint32_t *)&src->data[j * pixel_size + i * src->stride];
            *dst_pixel_ptr = *src_pixel_ptr;
        }
    }
}

void paint_blit_bitmap_rect(bitmap_t *dst, bitmap_t *src, int32_t src_x, int32_t src_y, uint32_t src_width, uint32_t src_height) {
    uint32_t pixel_size = bitmap_pixel_format_size(dst->pixel_format);

    for (uint32_t i = 0; i < src_height; i++) {
        for (uint32_t j = 0; j < src_width; j++) {
            int32_t x0 = src_x + j;
            int32_t y0 = src_y + i;

            if (x0 < 0 || y0 < 0 || x0 >= dst->width || y0 >= dst->height)
                continue;

            uint32_t *dst_pixel_ptr = (uint32_t *)&dst->data[x0 * pixel_size + y0 * dst->stride];
            uint32_t *src_pixel_ptr = (uint32_t *)&src->data[x0 * pixel_size + y0 * src->stride];
            *dst_pixel_ptr = *src_pixel_ptr;
        }
    }
}

static uint32_t blend_colors(uint32_t src_color, uint32_t dst_color, uint32_t alpha) {
    uint32_t inv_alpha = 255 - alpha;
    uint32_t src_red = (src_color >> 16) & 0xFF;
    uint32_t src_green = (src_color >> 8) & 0xFF;
    uint32_t src_blue = src_color & 0xFF;

    uint32_t dst_red = (dst_color >> 16) & 0xFF;
    uint32_t dst_green = (dst_color >> 8) & 0xFF;
    uint32_t dst_blue = dst_color & 0xFF;

    uint32_t red = (src_red * alpha + dst_red * inv_alpha) / 255;
    uint32_t green = (src_green * alpha + dst_green * inv_alpha) / 255;
    uint32_t blue = (src_blue * alpha + dst_blue * inv_alpha) / 255;

    return (red << 16) | (green << 8) | blue;
}

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
) {
    uint32_t pixel_size = bitmap_pixel_format_size(bitmap->pixel_format);
    
    for (uint32_t dy = 0; dy < dst_height; dy++) {
        int src_y0 = src_y + (dy * src_height) / dst_height;

        for (uint32_t dx = 0; dx < dst_width; dx++) {
            int src_x0 = src_x + (dx * src_width) / dst_width;
            uint8_t alpha = src_bitmap[src_x0 + src_y0 * src_stride];
            if (alpha == 0) continue;

            int dst_x0 = dst_x + dx;
            int dst_y0 = dst_y + dy;
            if (dst_x0 < 0 || dst_y0 < 0 || dst_x0 >= bitmap->width || dst_y0 >= bitmap->height)
                continue;

            uint32_t *pixel_ptr = (uint32_t *)&bitmap->data[dst_x0 * pixel_size + dst_y0 * bitmap->stride];
            uint32_t existing_color = *pixel_ptr;
            *pixel_ptr = blend_colors(color, existing_color, alpha);
        }
    }
}

void paint_swap_buffers(bitmap_t *bitmap, bitmap_t *back_bitmap) {
    assert(bitmap->pixel_format == back_bitmap->pixel_format);
    assert(bitmap->pixel_format == BITMAP_PIXEL_FORMAT_RGB32);

    uint32_t* bitmap_data = (uint32_t*)bitmap->data;
    uint32_t* back_bitmap_data = (uint32_t*)back_bitmap->data;
    uint32_t size = bitmap->width * bitmap->height;

    for (uint32_t i = 0; i < size; i++) {
        bitmap_data[i] = back_bitmap_data[i];
    }
}
