#pragma once

#include <stdint.h>

// Forward declarations.
typedef struct xhci_endpoint_t xhci_endpoint_t;
typedef struct usb_setup_packet_t usb_setup_packet_t;
typedef struct usb_endpoint_descriptor_t usb_endpoint_descriptor_t;
typedef struct xhci_input_context_t xhci_input_context_t;
typedef struct xhci_trb_t xhci_trb_t;

typedef struct xhci_trb_ring_t {
    // Volatile is important here - can cause stale reads if not used.
    volatile xhci_trb_t* base_address;
    volatile xhci_trb_t* enqueue_ptr;
    volatile xhci_trb_t* dequeue_ptr;
    volatile xhci_trb_t* link_trb;
    uint64_t ring_size;
    uint64_t entry_count;
    uint8_t cycle_bit;
} xhci_trb_ring_t;

typedef struct xhci_allocated_page_t {
    uint64_t virtual_address;
    uint64_t physical_address;
} xhci_allocated_page_t;

typedef struct xhci_device_t {
    uint8_t slot_id;
    uint8_t port_number;

    xhci_allocated_page_t input_context_base;
    xhci_input_context_t *input_context;
    xhci_trb_ring_t ep0_transfer_ring;
} xhci_device_t;

typedef struct xhci_endpoint_t {
    xhci_allocated_page_t transfer_ring_base;
    xhci_trb_ring_t transfer_ring;
    uint8_t endpoint_context_idx;
} xhci_endpoint_t;

typedef struct xhci_port_status_t {
    uint8_t port_number;
    uint8_t current_connect_status;
    uint8_t port_enabled;
} xhci_port_status_t;

typedef enum xhci_completion_code {
    XHCI_COMPLETION_CODE_SUCCESS = 0x1,
    XHCI_COMPLETION_CODE_FAILED = 0x0,
} xhci_completion_code;

typedef void (*xhci_transfer_callback_t)(
    xhci_completion_code completion_code,
    uint8_t *data,
    uint64_t size,
    void *callback_context
);

typedef enum xhci_control_transfer_type {
    NO_DATA,
    OUT_DATA,
    IN_DATA
} xhci_control_transfer_type;

typedef struct xhci_control_transfer_request_t {
    usb_setup_packet_t *setup_packet;
    xhci_transfer_callback_t callback;
    void *callback_context;

    // Configuraton
    uint8_t immediate_data_transfer;
    xhci_control_transfer_type transfer_type;
} xhci_control_transfer_request_t;

typedef struct xhci_transfer_request_t {
    xhci_endpoint_t *endpoint;
    xhci_transfer_callback_t callback;
    void *callback_context;

    // Configuration
    uint64_t transfer_length;
    uint8_t interrupt_on_short_packet;
} xhci_transfer_request_t;

int xhci_init(void);

// Watches for events on the event ring and handles them.
int xhci_poll_events(void);

uint8_t xhci_get_max_ports(void);
int xhci_get_port_status(uint8_t port_number, xhci_port_status_t *port_status);

int xhci_address_device(uint8_t port_number, xhci_device_t *out_device);
int xhci_control_transfer(xhci_device_t *device, xhci_control_transfer_request_t *request);
int xhci_configure_endpoint(
    xhci_device_t *device,
    usb_endpoint_descriptor_t *endpoint_descriptor,
    xhci_endpoint_t *endpoint
);
int xhci_transfer(xhci_device_t *device, xhci_transfer_request_t *request);
