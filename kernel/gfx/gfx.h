#pragma once

#include <stdint.h>
#include <libgfx/colors.h>

// Forward declarations.
typedef struct boot_info_t boot_info_t;
typedef struct bitmap_t bitmap_t;

int gfx_init(boot_info_t *boot_info);
