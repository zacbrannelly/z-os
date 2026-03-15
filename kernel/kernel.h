#pragma once

#include <stdint.h>

typedef struct boot_info {
    uint32_t *framebuffer;
    uint32_t framebuffer_size;
    uint32_t framebuffer_width;
    uint32_t framebuffer_stride;
} boot_info_t;
