#pragma once

#include "usb_core.h"

#define USB_HID_MOUSE_INTERFACE_CLASS    0x03
#define USB_HID_MOUSE_INTERFACE_SUBCLASS 0x01
#define USB_HID_MOUSE_INTERFACE_PROTOCOL 0x02

typedef struct usb_hid_mouse_report_t {
    uint8_t buttons;
    int8_t x;
    int8_t y;
    int8_t wheel;
} usb_hid_mouse_report_t;

// Initializes the USB HID mouse driver.
int usb_hid_mouse_init(void);

// Matches the USB HID mouse driver to a given interface.
usb_driver_match_result_t usb_hid_mouse_match(usb_interface_t *interface);

// Probes the USB HID mouse driver for a given interface.
int usb_hid_mouse_probe(usb_device_t *device, usb_configuration_t *configuration, usb_interface_t *interface);

// Polls for events from the USB HID mouse.
int usb_hid_mouse_poll(void);

// Gets the latest report from the USB HID mouse.
usb_hid_mouse_report_t *usb_hid_mouse_get_report(void);
