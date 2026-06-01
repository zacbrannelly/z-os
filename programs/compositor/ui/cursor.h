#pragma once

#include <stdint.h>

int cursor_init(uint32_t bounds_x, uint32_t bounds_y);

void cursor_set_position(int32_t x, int32_t y);
void cursor_get_position(int32_t *x, int32_t *y);
void cursor_move(int8_t dx, int8_t dy);

void cursor_set_bounds(uint32_t bounds_x, uint32_t bounds_y);
void cursor_get_bounds(uint32_t *bounds_x, uint32_t *bounds_y);

void cursor_update(void);
void cursor_draw(void);
