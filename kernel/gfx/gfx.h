#pragma once

#include <stdint.h>

#define GFX_COLOR_WHITE 0x00FFFFFF
#define GFX_COLOR_BLACK 0x00000000
#define GFX_COLOR(r, g, b) ((r << 16) | (g << 8) | b)

// Forward declarations.
typedef struct boot_info_t boot_info_t;

int gfx_init(boot_info_t *boot_info);
void gfx_clear(uint32_t color);
void gfx_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void gfx_swap_buffers(void);

uint32_t gfx_get_framebuffer_width(void);
uint32_t gfx_get_framebuffer_height(void);
