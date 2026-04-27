#pragma once

#include <stdint.h>

int font_init(void);
int font_draw_text(const char *text, uint32_t x, uint32_t baseline, uint32_t color);
