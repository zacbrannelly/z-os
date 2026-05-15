#include "bitmap.h"

#include "../kmalloc.h"
#include "../assert.h"
#include <stddef.h>

bitmap_t *bitmap_create(uint32_t width, uint32_t height, bitmap_pixel_format_t pixel_format) {
    bitmap_t *bitmap = (bitmap_t *)kmalloc(sizeof(bitmap_t));
    if (bitmap == NULL) {
        return NULL;
    }

    uint32_t stride = width * bitmap_pixel_format_size(pixel_format);
    bitmap->data = (uint8_t *)kmalloc(height * stride);
    if (bitmap->data == NULL) {
        kfree(bitmap);
        return NULL;
    }

    bitmap->width = width;
    bitmap->height = height;
    bitmap->stride = stride;
    bitmap->pixel_format = pixel_format;
    bitmap->is_data_owned = 1;

    return bitmap;
}

bitmap_t *bitmap_from_data(uint8_t *data, uint32_t width, uint32_t height, bitmap_pixel_format_t pixel_format) {
    bitmap_t *bitmap = (bitmap_t *)kmalloc(sizeof(bitmap_t));
    if (bitmap == NULL) {
        return NULL;
    }

    bitmap->data = data;
    bitmap->width = width;
    bitmap->height = height;
    bitmap->stride = width * bitmap_pixel_format_size(pixel_format);;
    bitmap->pixel_format = pixel_format;
    bitmap->is_data_owned = 0;

    return bitmap;
}

uint32_t bitmap_pixel_format_size(bitmap_pixel_format_t pixel_format) {
    switch (pixel_format) {
        case BITMAP_PIXEL_FORMAT_RGB32:
            return 4;
        default:
            assert(0);
    }

    __builtin_unreachable();
}

void bitmap_destroy(bitmap_t *bitmap) {
    if (bitmap->is_data_owned) {
        kfree(bitmap->data);
    }
    kfree(bitmap);
}
