#pragma once

#include <stdint.h>
#include "../pci/xhci.h"

// Descriptor types.
#define USB_DESCRIPTOR_TYPE_DEVICE 0x1
#define USB_DESCRIPTOR_TYPE_CONFIGURATION 0x2
#define USB_DESCRIPTOR_TYPE_INTERFACE 0x4
#define USB_DESCRIPTOR_TYPE_ENDPOINT 0x5

#define USB_MAX_NUM_DEVICES 16
#define USB_MAX_NUM_INTERFACES 16
#define USB_MAX_NUM_ENDPOINTS 32
#define USB_MAX_NUM_ALTERNATE_SETTINGS 16
#define USB_MAX_NUM_CONFIGURATIONS 8
#define USB_MAX_NUM_DRIVERS 256

// Forward declarations.
typedef struct xhci_device_t xhci_device_t;

#pragma pack(push, 1)

typedef struct usb_device_descriptor_t {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t bcdUSB;
    uint8_t bDeviceClass;
    uint8_t bDeviceSubClass;
    uint8_t bDeviceProtocol;
    uint8_t bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t iManufacturer;
    uint8_t iProduct;
    uint8_t iSerialNumber;
    uint8_t bNumConfigurations;
} usb_device_descriptor_t;

typedef struct usb_configuration_descriptor_t {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} usb_configuration_descriptor_t;

typedef struct usb_descriptor_header_t {
    uint8_t bLength;
    uint8_t bDescriptorType;
} usb_descriptor_header_t;

typedef struct usb_interface_descriptor_t {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} usb_interface_descriptor_t;

typedef struct usb_endpoint_descriptor_t {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} usb_endpoint_descriptor_t;

typedef struct usb_setup_packet_t {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_packet_t;

#pragma pack(pop)

typedef struct usb_endpoint_t {
    usb_endpoint_descriptor_t endpoint_descriptor;
    xhci_endpoint_t xhci_endpoint;
} usb_endpoint_t;

typedef struct usb_interface_alternate_setting_t {
    usb_interface_descriptor_t interface_descriptor;
    usb_endpoint_t endpoints[USB_MAX_NUM_ENDPOINTS];
    uint8_t num_endpoints;
} usb_interface_alternate_setting_t;

typedef struct usb_interface_t {
    uint8_t current_alternate_setting_idx;
    usb_interface_alternate_setting_t alternate_settings[USB_MAX_NUM_ALTERNATE_SETTINGS];
    uint8_t num_alternate_settings;
    uint8_t interface_number;
} usb_interface_t;

typedef struct usb_configuration_t {
    usb_configuration_descriptor_t configuration_descriptor;
    usb_interface_t interfaces[USB_MAX_NUM_INTERFACES];
    uint8_t num_interfaces;
} usb_configuration_t;

typedef struct usb_device_t {
    xhci_device_t xhci_device;
    usb_device_descriptor_t device_descriptor;
    usb_configuration_t configurations[USB_MAX_NUM_CONFIGURATIONS];
    uint8_t num_configurations;
} usb_device_t;

typedef enum usb_driver_match_result_t {
    USB_DRIVER_MATCH_ERROR = -1,
    USB_DRIVER_MATCH_NOT_FOUND = 0,
    USB_DRIVER_MATCH_FOUND = 1,
} usb_driver_match_result_t;

typedef struct usb_driver_t {
    usb_driver_match_result_t (*match)(usb_interface_t *interface);
    int (*probe)(usb_device_t *device, usb_configuration_t *configuration, usb_interface_t *interface);
} usb_driver_t;

// Initializes the USB driver.
int usb_init(void); 

// Enumerates the root hub and all connected devices.
int usb_enumerate_root_hub(void);

// Enumerates a single device for interfaces and endpoints.
int usb_enumerate_device(usb_device_t *device);

// Register a USB driver.
int usb_register_driver(usb_driver_t *driver);

// Iterate over all devices and their interfaces and bind drivers.
int usb_parse_interfaces(void);

int usb_get_descriptor(
    xhci_device_t *device,
    uint8_t descriptor_type,
    uint8_t descriptor_index,
    void *out_descriptor,
    uint64_t descriptor_length
);
int usb_get_device_descriptor(xhci_device_t *device, usb_device_descriptor_t *out_device_descriptor);
int usb_get_configuration_descriptor(
    xhci_device_t *device,
    uint8_t configuration_index,
    usb_configuration_descriptor_t *out_configuration_descriptor
);

// NOTE: Allocates a page for the data buffer. Must be freed by the caller.
int usb_get_all_configuration_descriptors(
    xhci_device_t *device,
    uint8_t configuration_index,
    usb_configuration_descriptor_t *in_configuration_descriptor,
    usb_descriptor_header_t **out_descriptors
);

int usb_find_descriptor_by_type(
    uint8_t descriptor_type,
    usb_configuration_descriptor_t *in_configuration_descriptor,
    usb_descriptor_header_t *in_descriptors,
    usb_descriptor_header_t **out_descriptor
);

int usb_set_configuration(xhci_device_t *device, uint8_t configuration_value);

int usb_configure_endpoint(usb_device_t *device, usb_endpoint_t *endpoint);
