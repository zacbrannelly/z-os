#pragma once

#include "../gfx/point.h"

typedef struct text_input_t {
    char *text;
    uint32_t length;
    uint32_t capacity;
    uint32_t cursor_index;

    point_t position;
    uint32_t cursor_color;
    uint32_t text_color;

    point_t cursor_position;
    point_t cursor_size;

    uint8_t key_down[6];
} text_input_t;

int text_input_init(text_input_t *text_input);
int text_input_destroy(text_input_t *text_input);
int text_input_add_char(text_input_t *text_input, char c);
int text_input_remove_char(text_input_t *text_input);
int text_input_move_cursor(text_input_t *text_input, int32_t dx);
int text_input_clear(text_input_t *text_input);
int text_input_get_text(text_input_t *text_input, char *text, uint32_t length);
int text_input_draw(text_input_t *text_input);
