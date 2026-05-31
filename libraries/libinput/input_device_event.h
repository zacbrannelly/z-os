#pragma once

#include <stdint.h>

typedef enum input_device_type_t {
    INPUT_DEVICE_TYPE_KEYBOARD,
    INPUT_DEVICE_TYPE_MOUSE,
} input_device_type_t;

typedef enum input_device_event_type_t {
    INPUT_DEVICE_EVENT_TYPE_KEY_DOWN_EVENT,
    INPUT_DEVICE_EVENT_TYPE_KEY_UP_EVENT,
    INPUT_DEVICE_EVENT_TYPE_MOUSE_DOWN_EVENT,
    INPUT_DEVICE_EVENT_TYPE_MOUSE_UP_EVENT,
    INPUT_DEVICE_EVENT_TYPE_MOUSE_MOVE_EVENT,
} input_device_event_type_t;

typedef struct input_device_key_event_t {
    uint8_t keycode;
} input_device_key_event_t;

typedef struct input_device_mouse_button_event_t {
    uint8_t button;
} input_device_mouse_button_event_t;

typedef struct input_device_mouse_move_event_t {
    int8_t delta_x;
    int8_t delta_y;
} input_device_mouse_move_event_t;

typedef struct input_device_event_t {
    input_device_event_type_t type;
    union {
        input_device_key_event_t key_event;
        input_device_mouse_button_event_t mouse_button_event;
        input_device_mouse_move_event_t mouse_move_event;
    };
} input_device_event_t;
