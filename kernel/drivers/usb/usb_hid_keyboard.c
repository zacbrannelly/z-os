#include "usb_hid_keyboard.h"

#include <stddef.h>
#include <libinput/keycodes.h>

#include "../../console.h"
#include "../../memory.h"
#include "../../input/input.h"
#include "../../input/input_device.h"

static usb_hid_keyboard_report_t g_latest_report;
static usb_endpoint_t *g_endpoint;
static usb_device_t *g_device;
static uint8_t g_latest_report_valid;
static uint8_t g_request_in_flight = 0;
static uint8_t g_request_cycle_bit = 0;

int usb_hid_keyboard_init(void) {
    g_latest_report_valid = 0;
    memory_set(&g_latest_report, 0, sizeof(g_latest_report));

    usb_driver_t driver = {
        .match = usb_hid_keyboard_match,
        .probe = usb_hid_keyboard_probe,
    };
    return usb_register_driver(&driver);
}

int usb_hid_keyboard_match(usb_interface_t *interface) {
    for (uint8_t i = 0; i < interface->num_alternate_settings; i++) {
        usb_interface_descriptor_t *interface_desc = &interface->alternate_settings[i].interface_descriptor;
        if (interface_desc->bInterfaceClass == USB_HID_KEYBOARD_INTERFACE_CLASS &&
            interface_desc->bInterfaceSubClass == USB_HID_KEYBOARD_INTERFACE_SUBCLASS &&
            interface_desc->bInterfaceProtocol == USB_HID_KEYBOARD_INTERFACE_PROTOCOL) {
            return USB_DRIVER_MATCH_FOUND;
        }
    }
    return USB_DRIVER_MATCH_NOT_FOUND;
}

static uint8_t report_contains_key(usb_hid_keyboard_report_t *report, uint8_t key) {
    for (uint8_t i = 0; i < 6; i++) {
        if (report->keypress[i] == key) {
            return 1;
        }
    }
    return 0;
}

static void usb_hid_keyboard_transfer_callback(
    xhci_completion_code completion_code,
    uint8_t *data,
    uint64_t size,
    void *callback_context
) {
    g_request_in_flight = 0;

    if (completion_code != XHCI_COMPLETION_CODE_SUCCESS) {
        console_write("Failed to transfer data from HID keyboard\r\n");
        return;
    }

    usb_hid_keyboard_report_t previous_report = g_latest_report;

    memory_copy(&g_latest_report, data, sizeof(g_latest_report) - 1);
    g_latest_report.cycle_bit = g_request_cycle_bit;
    g_latest_report_valid = 1;

    g_request_cycle_bit = !g_request_cycle_bit;

    // Emit the event on the input device.
    input_device_t *keyboard_device = input_get_keyboard_device();
    if (keyboard_device == NULL) return;

    // Emit KEY_DOWN_EVENT/KEY_UP_EVENT for each modifier that has changed.
    uint8_t changed_modifiers = previous_report.modifiers ^ g_latest_report.modifiers;
    while (changed_modifiers > 0) {
        int index = __builtin_ctz(changed_modifiers);
        uint8_t modifier = 1 << index;

        input_device_event_type_t event_type = (g_latest_report.modifiers & modifier) 
            ? INPUT_DEVICE_EVENT_TYPE_KEY_DOWN_EVENT 
            : INPUT_DEVICE_EVENT_TYPE_KEY_UP_EVENT;

        // KEY_LEFTCTRL is 0xe0, KEY_LEFTSHIFT is 0xe1, etc.
        // So we can add the index to the base keycode to get the actual keycode.
        input_device_key_event_t key_event = {
            .keycode = (KEY_LEFTCTRL + index),
        };

        input_device_event_t event = {
            .type = event_type,
            .key_event = key_event,
        };
        input_device_emit(keyboard_device, &event);

        // Clear the last set bit (so we can process the next modifier).
        changed_modifiers &= (changed_modifiers - 1);
    }

    // NOTE: g_latest_report.keypress is an unordered set of keys. 
    // So we can't rely on slot index to determine changes.
    for (uint8_t i = 0; i < 6; i++) {
        uint8_t prev_key = previous_report.keypress[i];
        uint8_t new_key = g_latest_report.keypress[i];

        // Detect KEY_UP_EVENT for keys that are no longer pressed.
        if (!report_contains_key(&g_latest_report, prev_key)) {
            input_device_key_event_t key_event = {
                .keycode = prev_key,
            };
            input_device_event_t event = {
                .type = INPUT_DEVICE_EVENT_TYPE_KEY_UP_EVENT,
                .key_event = key_event,
            };
            input_device_emit(keyboard_device, &event);
        }

        // Detect KEY_DOWN_EVENT for keys that are now pressed.
        if (!report_contains_key(&previous_report, new_key)) {
            input_device_key_event_t key_event = {
                .keycode = new_key,
            };
            input_device_event_t event = {
                .type = INPUT_DEVICE_EVENT_TYPE_KEY_DOWN_EVENT,
                .key_event = key_event,
            };
            input_device_emit(keyboard_device, &event);
        }
    }
}

int usb_hid_keyboard_probe(usb_device_t *device, usb_configuration_t *configuration, usb_interface_t *interface) {
    console_write("USB HID keyboard driver probed\r\n");

    uint8_t configuration_value = configuration->configuration_descriptor.bConfigurationValue;
    if (usb_set_configuration(&device->xhci_device, configuration_value) < 0) {
        console_write("Failed to set configuration\r\n");
        return -1;
    }

    usb_interface_alternate_setting_t *alternate_setting = &interface->alternate_settings[0];
    if (alternate_setting->num_endpoints != 1) {
        console_write("Invalid number of endpoints found for HID keyboard interface\r\n");
        return -1;
    }

    g_device = device;
    g_endpoint = &alternate_setting->endpoints[0];
    if (usb_configure_endpoint(device, g_endpoint) < 0) {
        console_write("Failed to configure endpoint\r\n");
        return -1;
    }

    xhci_transfer_request_t transfer_request = {
        .endpoint = &g_endpoint->xhci_endpoint,
        .callback = usb_hid_keyboard_transfer_callback,
        .callback_context = NULL,
        .transfer_length = sizeof(g_latest_report) - 1,
        .interrupt_on_short_packet = 1,
    };
    if (xhci_transfer(&device->xhci_device, &transfer_request) < 0) {
        console_write("Failed to start transfer\r\n");
        return -1;
    }
    g_request_in_flight = 1;

    return 0;
}

int usb_hid_keyboard_poll(void) {
    if (g_request_in_flight) {
        return 0;
    }

    xhci_transfer_request_t transfer_request = {
        .endpoint = &g_endpoint->xhci_endpoint,
        .callback = usb_hid_keyboard_transfer_callback,
        .callback_context = NULL,
        .transfer_length = sizeof(g_latest_report) - 1,
        .interrupt_on_short_packet = 1,
    };
    if (xhci_transfer(&g_device->xhci_device, &transfer_request) < 0) {
        console_write("Failed to start transfer\r\n");
        return -1;
    }
    g_request_in_flight = 1;

    return 0;
}

usb_hid_keyboard_report_t *usb_hid_keyboard_get_report(void) {
    if (!g_latest_report_valid) {
        return NULL;
    }
    return &g_latest_report;
}
