#pragma once

#include <stdint.h>

typedef enum bitmap_pixel_format_t {
    BITMAP_PIXEL_FORMAT_RGB32
} bitmap_pixel_format_t;

typedef struct bitmap_t {
    uint8_t *data;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    bitmap_pixel_format_t pixel_format;
    uint8_t is_data_owned;
} bitmap_t;

bitmap_t *bitmap_create(
    uint32_t width,
    uint32_t height,
    bitmap_pixel_format_t pixel_format
);

bitmap_t *bitmap_from_data(
    uint8_t *data,
    uint32_t width,
    uint32_t height,
    bitmap_pixel_format_t pixel_format
);

uint32_t bitmap_pixel_format_size(bitmap_pixel_format_t pixel_format);

void bitmap_destroy(bitmap_t *bitmap);
