#include "usb_core.h"
#include "../pci/xhci.h"
#include "../../memory.h"
#include "../../console.h"
#include "../../kmalloc.h"

#include <stddef.h>

#define REQUEST_RECIPIENT_DEVICE    0x0
#define REQUEST_RECIPIENT_INTERFACE 0x1
#define REQUEST_RECIPIENT_ENDPOINT  0x2
#define REQUEST_RECIPIENT_OTHER     0x3

#define REQUEST_TYPE_STANDARD 0x0
#define REQUEST_TYPE_CLASS    (0x1 << 5)
#define REQUEST_TYPE_VENDOR   (0x2 << 5)

#define REQUEST_DIR_HOST_TO_DEVICE (0 << 7)
#define REQUEST_DIR_DEVICE_TO_HOST (1 << 7)

#define GET_DESCRIPTOR_REQUEST 0x6
#define SET_CONFIGURATION_REQUEST 0x9

#define DEVICE_DESCRIPTOR_LENGTH 0x12
#define CONFIGURATION_DESCRIPTOR_LENGTH 0x9

#define USB_DATA_BUFFER_SIZE 4096

typedef struct usb_get_descriptor_context_t {
    uint8_t callback_called;
    void *out_descriptor;
    xhci_completion_code completion_code;
} usb_get_descriptor_context_t;

typedef struct usb_set_configuration_context_t {
    uint8_t callback_called;
    xhci_completion_code completion_code;
} usb_set_configuration_context_t;

typedef struct usb_core_driver_t {
    usb_device_t devices[USB_MAX_NUM_DEVICES];
    uint8_t num_devices;

    usb_driver_t drivers[USB_MAX_NUM_DRIVERS];
    uint8_t num_drivers;
} usb_core_driver_t;

static usb_core_driver_t g_usb_core_driver;

int usb_init(void) {
    memory_set(&g_usb_core_driver, 0, sizeof(g_usb_core_driver));

    if (usb_enumerate_root_hub() < 0) {
        console_write("Failed to enumerate root hub\r\n");
        return -1;
    }

    return 0;
}

int usb_register_driver(usb_driver_t *driver) {
    if (driver == NULL) {
        console_write("Input driver is NULL\r\n");
        return -1;
    }

    if (g_usb_core_driver.num_drivers >= USB_MAX_NUM_DRIVERS) {
        console_write("Max number of drivers reached\r\n");
        return -1;
    }

    g_usb_core_driver.drivers[g_usb_core_driver.num_drivers] = *driver;
    g_usb_core_driver.num_drivers++;

    return 0;
}

int usb_parse_interfaces(void) {
    for (uint8_t i = 0; i < g_usb_core_driver.num_devices; i++) {
        usb_device_t *device = &g_usb_core_driver.devices[i];

        for (uint8_t j = 0; j < device->num_configurations; j++) {
            usb_configuration_t *configuration = &device->configurations[j];

            for (uint8_t k = 0; k < configuration->num_interfaces; k++) {
                usb_interface_t *interface = &configuration->interfaces[k];

                if (interface->driver != NULL) {
                    continue;
                }

                for (uint8_t l = 0; l < g_usb_core_driver.num_drivers; l++) {
                    usb_driver_t *driver = &g_usb_core_driver.drivers[l];
                    if (driver->match(interface) == USB_DRIVER_MATCH_FOUND) {
                        interface->driver = driver;
                        driver->probe(device, configuration, interface);
                    }
                }
            }
        }
    }

    return 0;
}

static int alloc_device(usb_device_t **out_device) {
    if (out_device == NULL) {
        console_write("Output device is NULL\r\n");
        return -1;
    }

    usb_device_t *device = &g_usb_core_driver.devices[g_usb_core_driver.num_devices];
    memory_set(device, 0, sizeof(usb_device_t));

    g_usb_core_driver.num_devices++;

    *out_device = device;
    return 0;
}

int usb_enumerate_root_hub(void) {
    uint8_t max_ports = xhci_get_max_ports();
    for (uint8_t port = 0; port < max_ports; port++) {
        uint8_t port_number = port + 1;
        xhci_port_status_t port_status;
        if (xhci_get_port_status(port, &port_status) < 0) {
            console_write("Failed to get port status\r\n");
            return -1;
        }

        if (!port_status.current_connect_status || port_status.port_enabled) continue;

        usb_device_t *device = NULL;
        if (alloc_device(&device) < 0) {
            console_write("Failed to allocate device\r\n");
            return -1;
        }

        if (xhci_address_device(port_number, &device->xhci_device) < 0) {
            console_write("Failed to address device\r\n");
            return -1;
        }

        if (usb_enumerate_device(device) < 0) {
            console_write("Failed to enumerate device\r\n");
            return -1;
        }
    }
    return 0;
}

static int enumerate_configuration(usb_device_t *device, uint8_t configuration_index) {
    usb_configuration_t *config = &device->configurations[configuration_index];
    usb_configuration_descriptor_t *config_desc = &config->configuration_descriptor;
    if (usb_get_configuration_descriptor(&device->xhci_device, configuration_index, config_desc) < 0) {
        console_write("Failed to get configuration descriptor\r\n");
        return -1;
    }

    usb_descriptor_header_t *descriptors = NULL;
    if (usb_get_all_configuration_descriptors(&device->xhci_device, configuration_index, config_desc, &descriptors) < 0) {
        console_write("Failed to get all configuration descriptors\r\n");
        return -1;
    }

    usb_interface_descriptor_t *interface_desc = NULL;
    usb_interface_t *interface = NULL;
    usb_interface_alternate_setting_t *alternate_setting = NULL;

    uint64_t offset = 0;
    while (offset < config_desc->wTotalLength) {
        usb_descriptor_header_t *descriptor = (usb_descriptor_header_t *)((uint64_t)descriptors + offset);

        switch (descriptor->bDescriptorType) {
            case USB_DESCRIPTOR_TYPE_INTERFACE:
                interface_desc = (usb_interface_descriptor_t *)descriptor;

                if (interface == NULL || interface->interface_number != interface_desc->bInterfaceNumber) {
                    interface = &config->interfaces[config->num_interfaces];
                    config->num_interfaces++;

                    interface->interface_number = interface_desc->bInterfaceNumber;
                }

                alternate_setting = &interface->alternate_settings[interface->num_alternate_settings];
                interface->num_alternate_settings++;

                alternate_setting->interface_descriptor = *interface_desc;
                break;
            case USB_DESCRIPTOR_TYPE_ENDPOINT:
                usb_endpoint_descriptor_t *endpoint_desc = (usb_endpoint_descriptor_t *)descriptor;
                alternate_setting->endpoints[alternate_setting->num_endpoints].endpoint_descriptor = *endpoint_desc;
                alternate_setting->num_endpoints++;
                break;
            default:
                break;
        }
        
        offset += descriptor->bLength;
    }

    return 0;
}

int usb_enumerate_device(usb_device_t *device) {
    if (device == NULL) {
        console_write("Input device is NULL\r\n");
        return -1;
    }

    if (usb_get_device_descriptor(&device->xhci_device, &device->device_descriptor) < 0) {
        console_write("Failed to get device descriptor\r\n");
        return -1;
    }

    device->num_configurations = device->device_descriptor.bNumConfigurations;

    for (uint8_t i = 0; i < device->num_configurations; i++) {
        if (enumerate_configuration(device, i) < 0) {
            console_write("Failed to enumerate configuration\r\n");
            return -1;
        }
    }

    return 0;
}

int usb_get_device_descriptor(xhci_device_t *device, usb_device_descriptor_t *out_device_descriptor) {
    return usb_get_descriptor(device, USB_DESCRIPTOR_TYPE_DEVICE, 0, (void *)out_device_descriptor, DEVICE_DESCRIPTOR_LENGTH);
}

int usb_get_configuration_descriptor(xhci_device_t *device, uint8_t configuration_index, usb_configuration_descriptor_t *out_configuration_descriptor) {
    return usb_get_descriptor(device, USB_DESCRIPTOR_TYPE_CONFIGURATION, configuration_index, (void *)out_configuration_descriptor, CONFIGURATION_DESCRIPTOR_LENGTH);
}

int usb_get_all_configuration_descriptors(xhci_device_t *device, uint8_t configuration_index, usb_configuration_descriptor_t *in_configuration_descriptor, usb_descriptor_header_t **out_descriptors) {
    void *data_buffer = kmalloc(USB_DATA_BUFFER_SIZE);
    memory_set(data_buffer, 0, USB_DATA_BUFFER_SIZE);

    int result = usb_get_descriptor(device, USB_DESCRIPTOR_TYPE_CONFIGURATION, configuration_index, data_buffer, in_configuration_descriptor->wTotalLength);
    if (result < 0) {
        console_write("Failed to get configuration descriptor\r\n");
        kfree(data_buffer);
        return -1;
    }

    *out_descriptors = (usb_descriptor_header_t *)data_buffer;
    return 0;
}

static void usb_set_configuration_callback(
    xhci_completion_code completion_code,
    uint8_t *data,
    uint64_t size,
    void *callback_context
) {
    usb_set_configuration_context_t *context = (usb_set_configuration_context_t *)callback_context;
    context->callback_called = 1;
    context->completion_code = completion_code;
}

int usb_set_configuration(xhci_device_t *device, uint8_t configuration_value) {
    if (device == NULL) {
        console_write("Input assigned slot is NULL\r\n");
        return -1;
    }

    usb_set_configuration_context_t context = {
        .callback_called = 0,
        .completion_code = XHCI_COMPLETION_CODE_FAILED,
    };

    usb_setup_packet_t setup_packet = {
        .bmRequestType = REQUEST_DIR_HOST_TO_DEVICE | REQUEST_TYPE_STANDARD | REQUEST_RECIPIENT_DEVICE,
        .bRequest = SET_CONFIGURATION_REQUEST,
        .wValue = configuration_value,
        .wIndex = 0,
        .wLength = 0,
    };
    xhci_control_transfer_request_t request = {
        .setup_packet = &setup_packet,
        .callback = usb_set_configuration_callback,
        .callback_context = &context,
        .immediate_data_transfer = 1,
        .transfer_type = NO_DATA,
    };
    xhci_control_transfer(device, &request);

    while (!context.callback_called) {
        xhci_poll_events();
    }

    if (context.completion_code != XHCI_COMPLETION_CODE_SUCCESS) {
        console_write("Failed to set configuration\r\n");
        return -1;
    }

    return 0;
}

static void usb_get_descriptor_callback(
    xhci_completion_code completion_code,
    uint8_t *data,
    uint64_t size,
    void *callback_context
) {
    usb_get_descriptor_context_t *context = (usb_get_descriptor_context_t *)callback_context;
    context->callback_called = 1;

    context->completion_code = completion_code;
    if (completion_code != XHCI_COMPLETION_CODE_SUCCESS) return;

    // Copy the descriptor to the output buffer.
    memory_copy(context->out_descriptor, (void *)data, size);
}

int usb_get_descriptor(xhci_device_t *device, uint8_t descriptor_type, uint8_t descriptor_index, void *out_descriptor, uint64_t descriptor_length) {
    if (out_descriptor == NULL) {
        console_write("Output device descriptor is NULL\r\n");
        return -1;
    }

    usb_get_descriptor_context_t context = {
        .callback_called = 0,
        .out_descriptor = out_descriptor,
        .completion_code = XHCI_COMPLETION_CODE_FAILED,
    };

    usb_setup_packet_t setup_packet = {
        .bmRequestType = REQUEST_DIR_DEVICE_TO_HOST | REQUEST_TYPE_STANDARD | REQUEST_RECIPIENT_DEVICE,
        .bRequest = GET_DESCRIPTOR_REQUEST,
        .wValue = (descriptor_type << 8) | descriptor_index, // high = descriptor type (0x1), low = descriptor index (0x0)
        .wIndex = 0, // No index
        .wLength = descriptor_length,
    };
    xhci_control_transfer_request_t request = {
        .setup_packet = &setup_packet,
        .callback = usb_get_descriptor_callback,
        .callback_context = &context,
        .immediate_data_transfer = 1,
        .transfer_type = IN_DATA,
    };
    xhci_control_transfer(device, &request);

    while (!context.callback_called) {
        xhci_poll_events();
    }

    if (context.completion_code != XHCI_COMPLETION_CODE_SUCCESS) {
        console_write("Failed to get descriptor\r\n");
        return -1;
    }

    return 0;
}

int usb_configure_endpoint(usb_device_t *device, usb_endpoint_t *endpoint) {
    if (device == NULL) {
        console_write("Input device is NULL\r\n");
        return -1;
    }

    if (endpoint == NULL) {
        console_write("Input endpoint is NULL\r\n");
        return -1;
    }

    return xhci_configure_endpoint(
        &device->xhci_device,
        &endpoint->endpoint_descriptor,
        &endpoint->xhci_endpoint
    );
}
