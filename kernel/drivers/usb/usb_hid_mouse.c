#include "usb_hid_mouse.h"

#include <stddef.h>

#include "../../console.h"
#include "../../memory.h"
#include "../../input/input.h"
#include "../../input/input_device.h"

static usb_hid_mouse_report_t g_latest_report;
static usb_endpoint_t *g_endpoint;
static usb_device_t *g_device;
static uint8_t g_latest_report_valid;
static uint8_t g_request_in_flight = 0;
static uint8_t g_request_cycle_bit = 0;

int usb_hid_mouse_init(void) {
    g_latest_report_valid = 0;
    memory_set(&g_latest_report, 0, sizeof(g_latest_report));

    usb_driver_t driver = {
        .match = usb_hid_mouse_match,
        .probe = usb_hid_mouse_probe,
    };
    return usb_register_driver(&driver);
}

int usb_hid_mouse_match(usb_interface_t *interface) {
    for (uint8_t i = 0; i < interface->num_alternate_settings; i++) {
        usb_interface_descriptor_t *interface_desc = &interface->alternate_settings[i].interface_descriptor;
        if (interface_desc->bInterfaceClass == USB_HID_MOUSE_INTERFACE_CLASS &&
            interface_desc->bInterfaceSubClass == USB_HID_MOUSE_INTERFACE_SUBCLASS &&
            interface_desc->bInterfaceProtocol == USB_HID_MOUSE_INTERFACE_PROTOCOL) {
            return USB_DRIVER_MATCH_FOUND;
        }
    }
    return USB_DRIVER_MATCH_NOT_FOUND;
}

static void usb_hid_mouse_transfer_callback(
    xhci_completion_code completion_code,
    uint8_t *data,
    uint64_t size,
    void *callback_context
) {
    g_request_in_flight = 0;

    if (completion_code != XHCI_COMPLETION_CODE_SUCCESS) {
        console_write("Failed to transfer data from HID mouse\r\n");
        return;
    }

    usb_hid_mouse_report_t previous_report = g_latest_report;

    memory_copy(&g_latest_report, data, sizeof(g_latest_report) - 1);
    g_latest_report.cycle_bit = g_request_cycle_bit;
    g_latest_report_valid = 1;

    g_request_cycle_bit = !g_request_cycle_bit;

    // Emit the event on the input device.
    input_device_t *mouse_device = input_get_mouse_device();
    if (mouse_device == NULL) return;

    // Emit MOUSE_MOVE_EVENT if the mouse has moved.
    if (previous_report.x != g_latest_report.x || previous_report.y != g_latest_report.y) {
        input_device_mouse_move_event_t mouse_move_event = {
            .delta_x = g_latest_report.x,
            .delta_y = g_latest_report.y,
        };
        input_device_event_t event = {
            .type = INPUT_DEVICE_EVENT_TYPE_MOUSE_MOVE_EVENT,
            .mouse_move_event = mouse_move_event,
        };
        input_device_emit(mouse_device, &event);
    }

    // Emit MOUSE_UP_EVENT/MOUSE_DOWN_EVENT for each button that has changed.
    uint8_t changed_buttons = previous_report.buttons ^ g_latest_report.buttons;
    while (changed_buttons > 0) {
        // Get the index of the last set bit.
        int index = __builtin_ctz(changed_buttons);
        uint8_t button = 1 << index;

        input_device_event_type_t event_type = (g_latest_report.buttons & button) 
            ? INPUT_DEVICE_EVENT_TYPE_MOUSE_DOWN_EVENT 
            : INPUT_DEVICE_EVENT_TYPE_MOUSE_UP_EVENT;

        input_device_mouse_button_event_t mouse_button_event = {
            .button = button,
        };

        input_device_event_t event = {
            .type = event_type,
            .mouse_button_event = mouse_button_event,
        };
        input_device_emit(mouse_device, &event);

        // Clear the last set bit (so we can process the next button).
        changed_buttons &= (changed_buttons - 1);
    }
}

int usb_hid_mouse_probe(usb_device_t *device, usb_configuration_t *configuration, usb_interface_t *interface) {
    console_write("USB HID mouse driver probed\r\n");

    uint8_t configuration_value = configuration->configuration_descriptor.bConfigurationValue;
    if (usb_set_configuration(&device->xhci_device, configuration_value) < 0) {
        console_write("Failed to set configuration\r\n");
        return -1;
    }

    usb_interface_alternate_setting_t *alternate_setting = &interface->alternate_settings[0];
    if (alternate_setting->num_endpoints != 1) {
        console_write("Invalid number of endpoints found for HID mouse interface\r\n");
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
        .callback = usb_hid_mouse_transfer_callback,
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

int usb_hid_mouse_poll(void) {
    if (g_request_in_flight) {
        return 0;
    }

    xhci_transfer_request_t transfer_request = {
        .endpoint = &g_endpoint->xhci_endpoint,
        .callback = usb_hid_mouse_transfer_callback,
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

usb_hid_mouse_report_t *usb_hid_mouse_get_report(void) {
    if (!g_latest_report_valid) {
        return NULL;
    }
    return &g_latest_report;
}
