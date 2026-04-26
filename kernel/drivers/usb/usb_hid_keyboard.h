#pragma once

#include "usb_core.h"

#define USB_HID_KEYBOARD_INTERFACE_CLASS    0x03
#define USB_HID_KEYBOARD_INTERFACE_SUBCLASS 0x01
#define USB_HID_KEYBOARD_INTERFACE_PROTOCOL 0x01

typedef struct usb_hid_keyboard_report_t {
    uint8_t modifiers;
    uint8_t reserved;
    uint8_t keypress[6];
    uint8_t cycle_bit;
} usb_hid_keyboard_report_t;

// Initializes the USB HID keyboard driver.
int usb_hid_keyboard_init(void);

// Matches the USB HID keyboard driver to a given interface.
usb_driver_match_result_t usb_hid_keyboard_match(usb_interface_t *interface);

// Probes the USB HID keyboard driver for a given interface.
int usb_hid_keyboard_probe(usb_device_t *device, usb_configuration_t *configuration, usb_interface_t *interface);

// Polls for events from the USB HID keyboard.
int usb_hid_keyboard_poll(void);

// Gets the latest report from the USB HID keyboard.
usb_hid_keyboard_report_t *usb_hid_keyboard_get_report(void);
