#include "input.h"

#include <stddef.h>
#include <libz/handle.h>

#include "../files/file_table.h"
#include "../files/file.h"
#include "../process/input_file.h"

#include "../assert.h"
#include "input_device.h"

static input_device_t *g_mouse_device;
static input_device_t *g_keyboard_device;

int input_init(void) {
    // Create the device constructs.
    assert(input_device_create("/dev/mouse0", INPUT_DEVICE_TYPE_MOUSE, &g_mouse_device) == 0);
    assert(input_device_create("/dev/keyboard0", INPUT_DEVICE_TYPE_KEYBOARD, &g_keyboard_device) == 0);

    return 0;
}

// Get the mouse input device.
input_device_t *input_get_mouse_device(void) {
    return g_mouse_device;
}

// Get the keyboard input device.
input_device_t *input_get_keyboard_device(void) {
    return g_keyboard_device;
}
