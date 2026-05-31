#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct input_device_t input_device_t;

// Initialize the input system.
int input_init(void);

// Get the mouse input device.
input_device_t *input_get_mouse_device(void);

// Get the keyboard input device.
input_device_t *input_get_keyboard_device(void);
