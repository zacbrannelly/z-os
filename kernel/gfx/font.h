#pragma once

#include <stdint.h>

int font_init(void);
int font_get_line_height(void);
int font_get_ascent(void);
int font_get_descent(void);
int font_get_line_gap(void);
int font_calculate_cursor_pos(
    const char *text,
    uint32_t glyph_index,
    int32_t x,
    int32_t baseline,
    int32_t *out_cursor_x,
    int32_t *out_cursor_y
);
int font_draw_text(const char *text, int32_t x, int32_t baseline, uint32_t color);
