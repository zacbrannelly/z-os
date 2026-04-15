#include "xhci.h"
#include "pcie.h"
#include "../../memory.h"
#include "../../console.h"
#include "../../format.h"
#include "../../page_alloc.h"

#include <stdint.h>
#include <stddef.h>

// Cap register offsets.
#define XHCI_CAP_HCSPARAMS1 0x4
#define XHCI_CAP_RTSOFF 0x18
#define XHCI_CAP_DBOFF 0x14

// Op register offsets.
#define XHCI_OP_USBCMD 0x0
#define XHCI_OP_USBSTS 0x4
#define XHCI_OP_CRCR 0x18
#define XHCI_OP_DCBAAP 0x30
#define XHCI_OP_CONFIG 0x38
#define XHCI_OP_PORT_REGISTER(port, reg) (0x400 + (port * 16) + reg)

// Port register offsets.
#define XHCI_PORT_PORTSC    0x0
#define XHCI_PORT_PORTPMSC  0x4
#define XHCI_PORT_PORTLI    0x8
#define XHCI_PORT_PORTHLPMC 0xc

// Runtime register offsets.
#define XHCI_RT_ERSTSZ(x) (0x28 + 32 * x)
#define XHCI_RT_ERSTBA(x) (0x30 + 32 * x)
#define XHCI_RT_ERDP(x)   (0x38 + 32 * x)

// Op mask bits.
#define XHCI_OP_USBSTS_HCRST    (1 << 1)
#define XHCI_OP_USBSTS_CNR      (1 << 10)
#define XHCI_OP_USBCMD_RUN_STOP (1 << 0)

// TRB types
#define XHCI_TRB_TYPE(x)                         ((x) << 10)
#define XHCI_TRB_TYPE_NORMAL                     XHCI_TRB_TYPE(0x1)
#define XHCI_TRB_TYPE_LINK                       XHCI_TRB_TYPE(0x6)
#define XHCI_TRB_TYPE_ADDRESS_DEVICE_COMMAND     XHCI_TRB_TYPE(0xb)
#define XHCI_TRB_TYPE_NO_OP_COMMAND              XHCI_TRB_TYPE(0x17)
#define XHCI_TRB_TYPE_COMMAND_COMPLETION         XHCI_TRB_TYPE(0x21)
#define XHCI_TRB_TYPE_TRANSFER_EVENT             XHCI_TRB_TYPE(0x20)
#define XHCI_TRB_TYPE_ENABLE_SLOT_COMMAND        XHCI_TRB_TYPE(0x9)
#define XHCI_TRB_TYPE_CONFIGURE_ENDPOINT_COMMAND XHCI_TRB_TYPE(0xc)
#define XHCI_TRB_TYPE_SETUP_STAGE                XHCI_TRB_TYPE(0x2)
#define XHCI_TRB_TYPE_DATA_STAGE                 XHCI_TRB_TYPE(0x3)
#define XHCI_TRB_TYPE_STATUS_STAGE               XHCI_TRB_TYPE(0x4)

// TRB Control bits
#define XHCI_TRB_CONTROL_CYCLE_BIT (1 << 0)
#define XHCI_TRB_LINK_TOGGLE_CYLCE_BIT (1 << 1)

// Doorbell offsets.
#define XHCI_DOORBELL_CONTROLLER 0x0

typedef struct xhci_trb_t {
    uint32_t parameter_low;
    uint32_t parameter_high;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

typedef struct xhci_trb_ring_t {
    // Volatile is important here - can cause stale reads if not used.
    volatile xhci_trb_t* base_address;
    volatile xhci_trb_t* enqueue_ptr;
    volatile xhci_trb_t* dequeue_ptr;
    uint64_t ring_size;
    uint64_t entry_count;
    uint8_t cycle_bit;
} xhci_trb_ring_t;

typedef struct xhci_event_ring_segment_table_t {
    uint64_t ring_segment_base_address;
    uint32_t ring_segment_size;
    uint32_t reserved0;
} xhci_event_ring_segment_table_t;

typedef struct xhci_input_context_t {
    uint32_t drop_context_flags;
    uint32_t add_context_flags;
    uint32_t reserved0[5];
    uint8_t configuration_value;
    uint8_t interface_number;
    uint8_t alternate_setting;
    uint8_t reserved1;
} __attribute__((packed)) xhci_input_context_t;

typedef struct xhci_slot_context_t {
    // data0: Context Entries (5 bits), Hub (1 bit), MTT (1 bit), Reserved (1 bit), Speed (4 bits), Route String (20 bits)
    uint32_t data0;
    uint16_t max_exit_latency;
    uint8_t root_hub_port_number;
    uint8_t number_of_ports;
    uint8_t tt_hub_slot_id;
    uint8_t tt_port_number;
    // data1: Interrupter Target (10 bits), Reserved (4 bits), TTT (2 bits)
    uint16_t data1;
    uint8_t usb_device_address;
    uint16_t reserved0;
    // data2: Slot State (5 bits), Reserved (3 bits)
    uint8_t data2;
    uint32_t reserved1[4];
} __attribute__((packed)) xhci_slot_context_t;

#define SLOT_CONTEXT_SET_CONTEXT_ENTRIES(context, entries) ((context)->data0 |= (entries) << 27)
#define SLOT_CONTEXT_SET_HUB(context) ((context)->data0 |= (1 << 26))
#define SLOT_CONTEXT_SET_MTT(context) ((context)->data0 |= (1 << 25))
#define SLOT_CONTEXT_SET_SPEED(context, speed) ((context)->data0 |= ((speed) & 0xf) << 20)
#define SLOT_CONTEXT_SET_ROUTE_STRING(context, route_string) ((context)->data0 |= (route_string) << 0)
#define SLOT_CONTEXT_SET_INTERRUPTER_TARGET(context, target) ((context)->data1 |= (target) << 6)
#define SLOT_CONTEXT_SET_TTT(context, ttt) ((context)->data1 |= (ttt & 0x3) << 0)
#define SLOT_CONTEXT_SET_SLOT_STATE(context, state) ((context)->data2 |= (state & 0x1f) << 3)

typedef struct xhci_endpoint_context_t {
    uint8_t endpoint_state;
    uint8_t lsa_max_p_streams_mult;
    uint8_t interval;
    uint8_t max_esit_payload_high;
 
    uint8_t control; // HID, EP Type, CErr
    uint8_t max_burst_size;
    uint16_t max_packet_size;

    uint64_t transfer_ring_dequeue_ptr;

    uint16_t average_trb_length;
    uint16_t max_esit_payload_low;

    uint32_t reserved0[3];
} __attribute__((packed)) xhci_endpoint_context_t;

#define ENDPOINT_CONTEXT_SET_HID(context, hid) ((context)->control |= (hid & 0x1) << 7)
#define ENDPOINT_CONTEXT_SET_EP_TYPE(context, type) ((context)->control |= (type & 0x7) << 3)
#define ENDPOINT_CONTEXT_SET_CErr(context, c_err) ((context)->control |= (c_err & 0x3) << 1)

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
} __attribute__((packed)) usb_device_descriptor_t;

typedef struct usb_configuration_descriptor_t {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
} __attribute__((packed)) usb_configuration_descriptor_t;

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
} __attribute__((packed)) usb_interface_descriptor_t;

typedef struct usb_endpoint_descriptor_t {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t bInterval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

// Base address of the MMIO registers.
static volatile uint8_t *g_mmio_base = NULL;

// Base address of the capabilities registers.
static volatile uint8_t *g_caps = NULL;

// Length of the capabilities registers.
static uint8_t g_cap_length = 0;

// Base address of the operation registers.
static volatile uint8_t *g_ops = NULL;

// Base address of the runtime registers.
static volatile uint8_t *g_rt = NULL;

// Base address of the doorbell registers (Doorbell Array)
static volatile uint32_t *g_doorbells_array = NULL;

// PCIe bus, device, and function (BDF) numbers of the xHCI controller.
static uint32_t g_bus = 0;
static uint32_t g_device = 0;
static uint32_t g_function = 0;

static inline void xhci_data_sync_barrier(void) {
    __asm__ volatile("dsb sy" ::: "memory");
}

uint32_t xhci_read_op_register(uint32_t offset) {
    return *(volatile uint32_t*)(g_ops + offset);
}

void xhci_write_op_register(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(g_ops + offset) = value;
}

uint64_t xhci_read_op_register64(uint32_t offset) {
    return *(volatile uint64_t*)(g_ops + offset);
}

void xhci_write_op_register64(uint32_t offset, uint64_t value) {
    *(volatile uint64_t*)(g_ops + offset) = value;
}

uint32_t xhci_read_cap_register(uint32_t offset) {
    return *(volatile uint32_t*)(g_caps + offset);
}

void xhci_write_cap_register(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(g_caps + offset) = value;
}

uint64_t xhci_read_cap_register64(uint32_t offset) {
    return *(volatile uint64_t*)(g_caps + offset);
}

void xhci_write_cap_register64(uint32_t offset, uint64_t value) {
    *(volatile uint64_t*)(g_caps + offset) = value;
}

uint32_t xhci_read_rt_register(uint32_t offset) {
    return *(volatile uint32_t*)(g_rt + offset);
}

void xhci_write_rt_register(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(g_rt + offset) = value;
}

uint64_t xhci_read_rt_register64(uint32_t offset) {
    return *(volatile uint64_t*)(g_rt + offset);
}

void xhci_write_rt_register64(uint32_t offset, uint64_t value) {
    *(volatile uint64_t*)(g_rt + offset) = value;
}

uint32_t xhci_read_port_register(uint8_t port, uint32_t offset) {
    return xhci_read_op_register(XHCI_OP_PORT_REGISTER(port, offset));
}

void xhci_write_port_register(uint8_t port, uint32_t offset, uint32_t value) {
    xhci_write_op_register(XHCI_OP_PORT_REGISTER(port, offset), value);
}

uint64_t xhci_read_port_register64(uint8_t port, uint32_t offset) {
    return xhci_read_op_register64(XHCI_OP_PORT_REGISTER(port, offset));
}

void xhci_write_port_register64(uint8_t port, uint32_t offset, uint64_t value) {
    xhci_write_op_register64(XHCI_OP_PORT_REGISTER(port, offset), value);
}

int xhci_is_halted(void) {
    uint32_t usb_sts = xhci_read_op_register(XHCI_OP_USBSTS);
    return (usb_sts & 0x1) != 0;
}

void xhci_wait_for_reset(void) {
    // Spin until the HCRST bit is cleared, this will clear when the controller is reset.
    while ((xhci_read_op_register(XHCI_OP_USBCMD) & XHCI_OP_USBSTS_HCRST) > 0) {}
}

void xhci_wait_for_controller_ready(void) {
    // Spin until the CNR bit is clear, this will clear when the controller is ready.
    while ((xhci_read_op_register(XHCI_OP_USBSTS) & XHCI_OP_USBSTS_CNR) != 0) {}
}

int xhci_locate_device(void) {
    int pcie_search_result = pcie_locate_device(
        0x0,
        PCI_CLASS_SERIAL_BUS_CONTROLLER,
        PCI_SUBCLASS_USB_CONTROLLER,
        PCI_PROG_INT_USB_3_0_XHCI,
        &g_bus,
        &g_device,
        &g_function
    );
    if (pcie_search_result != 0) {
        return -1;
    }

    char buffer[17];
    console_write("xHCI controller found at bus: 0x");
    format_hex(buffer, sizeof(buffer), g_bus);
    console_write(buffer);
    console_write(" device: 0x");
    format_hex(buffer, sizeof(buffer), g_device);
    console_write(buffer);
    console_write(" function: 0x");
    format_hex(buffer, sizeof(buffer), g_function);
    console_write(buffer);
    console_write("\r\n");

    uint32_t bar0 = pcie_config_read32(g_bus, g_device, g_function, PCI_CONFIG_BAR0);
    uint32_t bar1 = pcie_config_read32(g_bus, g_device, g_function, PCI_CONFIG_BAR1);
    
    // TODO: Map this address space to the kernel's virtual address space.
    // TODO: Make sure to map this as device memory (not normal memory).
    uint64_t base_address_low = (uint64_t)(bar0 & 0xfffffff0);
    uint64_t base_address_high = (uint64_t)(bar1 & 0xfffffff0);
    uint64_t base_address = (base_address_high << 32) | base_address_low;

    console_write("xHCI base address: 0x");
    format_hex(buffer, sizeof(buffer), base_address);
    console_write(buffer);
    console_write("\r\n");

    uint16_t command = pcie_config_read16(g_bus, g_device, g_function, PCI_CONFIG_COMMAND);
    console_write("xHCI command: 0x");
    format_hex(buffer, sizeof(buffer), command);
    console_write(buffer);
    console_write("\r\n");

    // Enable the Bus Master and Memory Space capabilities.
    pcie_config_write16(g_bus, g_device, g_function, PCI_CONFIG_COMMAND, 0x6);

    g_mmio_base = (uint8_t *)base_address;
    g_caps = g_mmio_base;
    g_cap_length = g_mmio_base[0];
    g_ops = g_mmio_base + g_cap_length;

    // Read the RTSOFF register to find the runtime registers base address.
    uint32_t rtsoff = xhci_read_cap_register(XHCI_CAP_RTSOFF);
    g_rt = g_caps + rtsoff;

    // Read the DBOFF register to find the doorbell registers base address.
    uint32_t dboff = xhci_read_cap_register(XHCI_CAP_DBOFF);
    g_doorbells_array = (uint32_t *)(g_caps + dboff);

    return 0;
}

int xhci_command_ring_init(xhci_trb_ring_t *command_ring, uint64_t base_address, uint64_t ring_size) {
    command_ring->base_address = (xhci_trb_t*)base_address;
    command_ring->enqueue_ptr = command_ring->base_address;
    command_ring->ring_size = ring_size;
    command_ring->entry_count = 0;
    command_ring->cycle_bit = 1;
    return 0;
}

int xhci_command_ring_enqueue(xhci_trb_ring_t *command_ring, xhci_trb_t *trb) {
    // Ring is full, flip the cycle bit and reset the enqueue pointer.
    if (command_ring->entry_count >= command_ring->ring_size) {
        command_ring->cycle_bit = !command_ring->cycle_bit;
        command_ring->enqueue_ptr = command_ring->base_address;
        command_ring->entry_count = 0;
    }

    // Set the cycle bit in the control field of the TRB.
    if (command_ring->cycle_bit == 1) {
        trb->control |= 1;
    } else {
        trb->control &= ~1;
    }

    *command_ring->enqueue_ptr = *trb;
    command_ring->enqueue_ptr++;
    command_ring->entry_count++;
    return 0;
}

void xhci_ring_command_doorbell(void) {
    xhci_data_sync_barrier();
    g_doorbells_array[XHCI_DOORBELL_CONTROLLER] = 0;
}

void xhci_ring_doorbell(uint32_t doorbell_idx, uint32_t target) {
    xhci_data_sync_barrier();
    g_doorbells_array[doorbell_idx] = target;
}

int xhci_event_ring_init(xhci_trb_ring_t *event_ring, uint64_t base_address, uint64_t ring_size) {
    event_ring->base_address = (xhci_trb_t*)base_address;
    event_ring->dequeue_ptr = event_ring->base_address;
    event_ring->ring_size = ring_size;
    event_ring->entry_count = 0;
    event_ring->cycle_bit = 1;
    return 0;
}

int xhci_event_ring_has_valid_trb(xhci_trb_ring_t *event_ring) {
    // Event is valid if the cycle bit matches the expected cycle bit.
    return (event_ring->dequeue_ptr->control & XHCI_TRB_CONTROL_CYCLE_BIT) == event_ring->cycle_bit;
}

int xhci_event_ring_dequeue(xhci_trb_ring_t *event_ring, xhci_trb_t *trb) {
    if (!xhci_event_ring_has_valid_trb(event_ring)) {
        return -1;
    }

    *trb = *event_ring->dequeue_ptr;
    event_ring->dequeue_ptr++;
    event_ring->entry_count++;

    // Check for loop of the ring - flip the cycle bit and reset the dequeue pointer if the ring is full.
    if (event_ring->entry_count >= event_ring->ring_size) {
        event_ring->cycle_bit = !event_ring->cycle_bit;
        event_ring->dequeue_ptr = event_ring->base_address;
        event_ring->entry_count = 0;
    }

    // Write the new dequeue pointer to the ERDP register - signals we've consumed a TRB.
    // TODO: Make the index configurable.
    xhci_write_rt_register64(XHCI_RT_ERDP(0), (uint64_t)event_ring->dequeue_ptr | (1 << 3));

    return 0;
}

void xhci_event_ring_wait_for_event(xhci_trb_ring_t *event_ring) {
    // Spin until the event ring has a valid TRB.
    while (!xhci_event_ring_has_valid_trb(event_ring)) {}
}

int xhci_trb_is_type(xhci_trb_t *trb, uint32_t type_mask) {
    return ((trb->control >> 10) & 0x3f) == (type_mask >> 10);
}

int xhci_init(void) {
    if (xhci_locate_device() != 0) {
        return -1;
    }

    uint32_t hcs_params_s1 = xhci_read_cap_register(XHCI_CAP_HCSPARAMS1);
    console_write("xHCI HCS params S1: ");
    console_write_hex(hcs_params_s1);
    console_write("\r\n");

    uint8_t max_device_slots = hcs_params_s1 & 0xff;
    uint8_t max_ports = (hcs_params_s1 >> 24) & 0xff;

    console_write("xHCI max device slots: ");
    console_write_hex(max_device_slots);
    console_write("\r\n");

    console_write("xHCI max ports: ");
    console_write_hex(max_ports);
    console_write("\r\n");

    uint32_t usb_cmd = xhci_read_op_register(XHCI_OP_USBCMD);
    console_write("xHCI USB CMD: ");
    console_write_hex(usb_cmd);
    console_write("\r\n");

    uint32_t usb_sts = xhci_read_op_register(XHCI_OP_USBSTS);
    console_write("xHCI USB STS: ");
    console_write_hex(usb_sts);
    console_write("\r\n");

    // Reset the controller (set the USBCMD.HCRST bit)
    xhci_write_op_register(XHCI_OP_USBCMD, usb_cmd | XHCI_OP_USBSTS_HCRST);
    xhci_wait_for_reset();

    console_write("xHCI controller reset complete\r\n");

    // Set Max Device Slots Enabled in the CONFIG register.
    uint32_t config = xhci_read_op_register(XHCI_OP_CONFIG);
    xhci_write_op_register(XHCI_OP_CONFIG, config | (max_device_slots & 0xff));

    // Allocate a DCBA and write the address to the DCBAAP register.
    uint64_t dcba = 0;
    if (page_alloc_block(PAGE_ALLOC_ORDER_4KB, &dcba) < 0) {
        console_write("Failed to allocate DCBA\r\n");
        return -1;
    }
    if (dcba % (1 << 6) != 0) {
        console_write("Allocated DCBA is not aligned to 64 bytes\r\n");
        return -1;
    }

    // Zero out the DCBA.
    memory_set((void *)dcba, 0, 4096);

    xhci_write_op_register64(XHCI_OP_DCBAAP, dcba);

    // Allocate a command ring base.
    uint64_t command_ring_base = 0;
    if (page_alloc_block(PAGE_ALLOC_ORDER_4KB, &command_ring_base) < 0) {
        console_write("Failed to allocate command ring base\r\n");
        return -1;
    }
    if (command_ring_base % (1 << 6) != 0) {
        console_write("Allocated command ring base is not aligned to 64 bytes\r\n");
        return -1;
    }

    // Zero out the command ring.
    memory_set((void *)command_ring_base, 0, 4096);

    // Write the command ring base to the CRCR register and set the CYCLE bit.
    xhci_write_op_register64(XHCI_OP_CRCR, command_ring_base | 0x1);

    // Initialize the command ring.
    xhci_trb_ring_t command_ring;
    xhci_command_ring_init(&command_ring, command_ring_base, 2);

    // Enqueue a NO-OP command TRB.
    xhci_trb_t no_op_command_trb = {
        .parameter_low = 0,
        .parameter_high = 0,
        .status = 0,
        .control = XHCI_TRB_TYPE_NO_OP_COMMAND,
    };
    xhci_command_ring_enqueue(&command_ring, &no_op_command_trb);

    // Enqueue a LINK command TRB -- Links back to the start of the same ring, toggles the cycle bit.
    xhci_trb_t link_command_trb = {
        .parameter_low = (uint32_t)command_ring_base,
        .parameter_high = (uint32_t)(command_ring_base >> 32),
        .status = 0,
        .control = XHCI_TRB_TYPE_LINK | XHCI_TRB_LINK_TOGGLE_CYLCE_BIT,
    };
    xhci_command_ring_enqueue(&command_ring, &link_command_trb);

    // Allocate memory for the ERST table.
    uint64_t event_ring_segment_table_base = 0;
    if (page_alloc_block(PAGE_ALLOC_ORDER_4KB, &event_ring_segment_table_base) < 0) {
        console_write("Failed to allocate event ring base\r\n");
        return -1;
    }
    if (event_ring_segment_table_base % (1 << 6) != 0) {
        console_write("Allocated event ring base is not aligned to 64 bytes\r\n");
        return -1;
    }

    // Zero out the ERST table.
    memory_set((void *)event_ring_segment_table_base, 0, 4096);

    // Allocate memory for the event ring segment.
    uint64_t event_ring_segment_base = 0;
    if (page_alloc_block(PAGE_ALLOC_ORDER_4KB, &event_ring_segment_base) < 0) {
        console_write("Failed to allocate event ring segment base\r\n");
        return -1;
    }
    if (event_ring_segment_base % (1 << 6) != 0) {
        console_write("Allocated event ring segment base is not aligned to 64 bytes\r\n");
        return -1;
    }

    // Zero out the event ring segment.
    memory_set((void *)event_ring_segment_base, 0, 4096);

    // Create ERST table with a single entry mapping to a single event ring segment.
    xhci_event_ring_segment_table_t *erst = (xhci_event_ring_segment_table_t *)event_ring_segment_table_base;
    erst->ring_segment_base_address = event_ring_segment_base;
    erst->ring_segment_size = 256;

    // Initialize the event ring.
    xhci_trb_ring_t event_ring;
    xhci_event_ring_init(&event_ring, event_ring_segment_base, 256);

    // Write the ERST table size to the ERSTSZ register.
    xhci_write_rt_register(XHCI_RT_ERSTSZ(0), 1);
    
    // Write the ERDP (dequeue pointer) register to the event ring segment base address.
    xhci_write_rt_register64(XHCI_RT_ERDP(0), event_ring_segment_base);

    // Write the ERST table base address to the first ERSTBA register.
    // NOTE: This write enables the event ring.
    xhci_write_rt_register64(XHCI_RT_ERSTBA(0), event_ring_segment_table_base);

    // Enable the xHCI controller.
    usb_cmd = xhci_read_op_register(XHCI_OP_USBCMD);
    xhci_write_op_register(XHCI_OP_USBCMD, usb_cmd | XHCI_OP_USBCMD_RUN_STOP);

    // Wait for the controller to be ready.
    xhci_wait_for_controller_ready();
    xhci_wait_for_reset();
    console_write("xHCI controller ready\r\n");

    // Send a doorbell to the controller.
    xhci_ring_command_doorbell();

    // Wait for an event to be available on the event ring.
    xhci_event_ring_wait_for_event(&event_ring);
    console_write("Event ring has a valid TRB\r\n");

    // Dequeue the event.
    xhci_trb_t event_trb;
    xhci_event_ring_dequeue(&event_ring, &event_trb);

    console_write("Event TRB: ");
    console_write("Low: ");
    console_write_hex(event_trb.parameter_low);
    console_write(" High: ");
    console_write_hex(event_trb.parameter_high);
    console_write(" Status: ");
    console_write_hex(event_trb.status);
    console_write(" Control: ");
    console_write_hex(event_trb.control);
    console_write("\r\n");

    if (!xhci_trb_is_type(&event_trb, XHCI_TRB_TYPE_COMMAND_COMPLETION)) {
        console_write("Event TRB is not a command completion\r\n");
        return -1;
    }

    uint8_t completion_code = (event_trb.status >> 24) & 0xff;
    if (completion_code != 1) {
        console_write("Command failed with completion code: ");
        console_write_hex(completion_code);
        console_write("\r\n");
        return -1;
    }

    for (uint8_t port = 0; port < max_ports; port++) {
        uint32_t port_sc = xhci_read_port_register(port, XHCI_PORT_PORTSC);
        console_write("Port ");
        console_write_hex(port);
        console_write(" SC: ");
        console_write_hex(port_sc);
        console_write("\r\n");
    }

    xhci_trb_t enable_slot_command = {
        .parameter_low = 0,
        .parameter_high = 0,
        .status = 0,
        .control = XHCI_TRB_TYPE_ENABLE_SLOT_COMMAND,
    };
    xhci_command_ring_enqueue(&command_ring, &enable_slot_command);

    // Enqueue a LINK command TRB -- Links back to the start of the same ring, toggles the cycle bit.
    xhci_command_ring_enqueue(&command_ring, &link_command_trb);
    xhci_ring_command_doorbell();

    // Wait for an event to be available on the event ring.
    xhci_event_ring_wait_for_event(&event_ring);
    console_write("Event ring has a valid TRB\r\n");

    // Dequeue the event.
    xhci_event_ring_dequeue(&event_ring, &event_trb);
    
    if (!xhci_trb_is_type(&event_trb, XHCI_TRB_TYPE_COMMAND_COMPLETION)) {
        console_write("Event TRB is not a command completion\r\n");
        return -1;
    }

    completion_code = (event_trb.status >> 24) & 0xff;
    if (completion_code != 1) {
        console_write("Command failed with completion code: ");
        console_write_hex(completion_code);
        console_write("\r\n");
        return -1;
    }

    // Extract the slot ID from the completion event TRB.
    uint8_t slot_id = (event_trb.control >> 24) & 0xff;
    console_write("Slot ID: ");
    console_write_hex(slot_id);
    console_write("\r\n");

    // Allocate a page for the slot context in the DCBA table.
    uint64_t *dcba_table = (uint64_t *)dcba;
    if (page_alloc_block(PAGE_ALLOC_ORDER_4KB, &dcba_table[slot_id]) < 0) {
        console_write("Failed to allocate slot context\r\n");
        return -1;
    }
    memory_set((void *)dcba_table[slot_id], 0, 4096);

    uint64_t input_context_base = 0;
    if (page_alloc_block(PAGE_ALLOC_ORDER_4KB, &input_context_base) < 0) {
        console_write("Failed to allocate input context\r\n");
        return -1;
    }
    memory_set((void *)input_context_base, 0, 4096);

    // Set A0 = "Slot Context" and A1 = "Endpoint Context 0" in the input control context.
    *((uint32_t *)input_context_base + 1) = 0x3; // 0b11

    // Input Slot Context.
    uint32_t *input_slot_context = (uint32_t *)(input_context_base + 8 * sizeof(uint32_t));
    input_slot_context[0] = 0x1 << 27; // Context Entries = 0x1, Route String = 0x0
    input_slot_context[1] = 0x6 << 16; // Root Hub Port = 0x5 (which maps to Port 0x4, since this is 1-indexed)

    // Input Endpoint Context 0.
    xhci_endpoint_context_t *endpoint_context = (xhci_endpoint_context_t *)(input_context_base + 0x40);
    endpoint_context->max_packet_size = 64;
    endpoint_context->control = 0x4 << 3 | 0x3 << 1; // EP Type = "Control" (0x4)

    uint64_t transfer_ring_base = 0;
    if (page_alloc_block(PAGE_ALLOC_ORDER_4KB, &transfer_ring_base) < 0) {
        console_write("Failed to allocate transfer ring base\r\n");
        return -1;
    }
    if (transfer_ring_base % (1 << 6) != 0) {
        console_write("Allocated transfer ring base is not aligned to 64 bytes\r\n");
        return -1;
    }
    memory_set((void *)transfer_ring_base, 0, 4096);

    endpoint_context->transfer_ring_dequeue_ptr = transfer_ring_base | 0x1; // Set the cycle bit.

    xhci_trb_t address_device_command = {
        .parameter_low = (uint32_t)input_context_base,
        .parameter_high = (uint32_t)(input_context_base >> 32),
        .status = 0,
        .control = XHCI_TRB_TYPE_ADDRESS_DEVICE_COMMAND | (slot_id << 24),
    };
    xhci_command_ring_enqueue(&command_ring, &address_device_command);

    // Enqueue a LINK command TRB -- Links back to the start of the same ring, toggles the cycle bit.
    xhci_command_ring_enqueue(&command_ring, &link_command_trb);
    xhci_ring_command_doorbell();

    // Wait for an event to be available on the event ring.
    xhci_event_ring_wait_for_event(&event_ring);
    console_write("Event ring has a valid TRB\r\n");

    // Dequeue the event.
    xhci_event_ring_dequeue(&event_ring, &event_trb);
    
    if (!xhci_trb_is_type(&event_trb, XHCI_TRB_TYPE_COMMAND_COMPLETION)) {
        console_write("Event TRB is not a command completion\r\n");
        return -1;
    }
    
    completion_code = (event_trb.status >> 24) & 0xff;
    if (completion_code != 1) {
        console_write("Command failed with completion code: ");
        console_write_hex(completion_code);
        console_write("\r\n");
        return -1;
    }

    console_write("Address device command completed successfully\r\n");

    xhci_trb_ring_t ep0_transfer_ring;
    xhci_command_ring_init(&ep0_transfer_ring, transfer_ring_base, 256);

    xhci_trb_t setup_packet_trb = {
        // bmRequestType = 0x80 (Dir = Device to Host, Type = Standard, Recipient = Device)
        // bRequest = GET_DESCRIPTOR (0x6)
        // wValue = Device Descriptor (0x1)
        .parameter_low = 0x80 | (0x6 << 8) | (0x1 << 24),
        // wIndex = 0 (no index)
        // wLength = 0x12 (18 bytes)
        .parameter_high = (0x12 << 16),
        .status = 0x8, // TRB Size = 0x8 (8 bytes, ALWAYS), Interrupter Target = 0x0
        .control = (1 << 6) | (0x3 << 16) | XHCI_TRB_TYPE_SETUP_STAGE // IDT (Immediate data transfer), Transfer Type = IN Data Stage (0x3), SETUP_STAGE (0x2)
    };
    xhci_command_ring_enqueue(&ep0_transfer_ring, &setup_packet_trb);

    // Allocate a data stage buffer.
    uint64_t data_stage_buffer = 0;
    if (page_alloc_block(PAGE_ALLOC_ORDER_4KB, &data_stage_buffer) < 0) {
        console_write("Failed to allocate data stage buffer\r\n");
        return -1;
    }
    memory_set((void *)data_stage_buffer, 0, 4096);

    // Enqueue data stage TRB.
    xhci_trb_t data_stage_trb = {
        .parameter_low = (uint32_t)data_stage_buffer,
        .parameter_high = (uint32_t)(data_stage_buffer >> 32),
        .status = 0x12, // Transfer Size = 0x12 (18 bytes)
        .control = (1 << 16) | XHCI_TRB_TYPE_DATA_STAGE, // DIR = 1, IDT = 0, IOC = 0, CH = 0
    };
    xhci_command_ring_enqueue(&ep0_transfer_ring, &data_stage_trb);

    // Enqueue status stage TRB.
    xhci_trb_t status_stage_trb = {
        .parameter_low = 0,
        .parameter_high = 0,
        .status = 0x0,
        .control = (1 << 5) | XHCI_TRB_TYPE_STATUS_STAGE, // IOC = 1, 
    };
    xhci_command_ring_enqueue(&ep0_transfer_ring, &status_stage_trb);

    // Ring the transfer ring doorbell.
    xhci_ring_doorbell(slot_id, 0x1); // 0x1 = EP0 Enqueue Pointer Update

    xhci_event_ring_wait_for_event(&event_ring);
    console_write("Event ring has a valid TRB\r\n");

    // Dequeue the event.
    xhci_event_ring_dequeue(&event_ring, &event_trb);
    
    if (!xhci_trb_is_type(&event_trb, XHCI_TRB_TYPE_TRANSFER_EVENT)) {
        console_write("Event TRB is not a transfer event\r\n");
        return -1;
    }

    completion_code = (event_trb.status >> 24) & 0xff;
    if (completion_code != 1) {
        console_write("Transfer event failed with completion code: ");
        console_write_hex(completion_code);
        console_write("\r\n");
        return -1;
    }

    console_write("Setup packet transfer completed successfully\r\n");

    usb_device_descriptor_t *device_descriptor = (usb_device_descriptor_t *)data_stage_buffer;
    console_write("Device Descriptor: \r\n");
    console_write("bLength: ");
    console_write_hex(device_descriptor->bLength);
    console_write("\r\n");
    console_write("bDescriptorType: ");
    console_write_hex(device_descriptor->bDescriptorType);
    console_write("\r\n");
    console_write("bcdUSB: ");
    console_write_hex(device_descriptor->bcdUSB);
    console_write("\r\n");
    console_write("bDeviceClass: ");
    console_write_hex(device_descriptor->bDeviceClass);
    console_write("\r\n");
    console_write("bDeviceSubClass: ");
    console_write_hex(device_descriptor->bDeviceSubClass);
    console_write("\r\n");
    console_write("bDeviceProtocol: ");
    console_write_hex(device_descriptor->bDeviceProtocol);
    console_write("\r\n");
    console_write("bMaxPacketSize0: ");
    console_write_hex(device_descriptor->bMaxPacketSize0);
    console_write("\r\n");
    console_write("idVendor: ");
    console_write_hex(device_descriptor->idVendor);
    console_write("\r\n");
    console_write("idProduct: ");
    console_write_hex(device_descriptor->idProduct);
    console_write("\r\n");
    console_write("bcdDevice: ");
    console_write_hex(device_descriptor->bcdDevice);
    console_write("\r\n");
    console_write("iManufacturer: ");
    console_write_hex(device_descriptor->iManufacturer);
    console_write("\r\n");
    console_write("iProduct: ");
    console_write_hex(device_descriptor->iProduct);
    console_write("\r\n");
    console_write("iSerialNumber: ");
    console_write_hex(device_descriptor->iSerialNumber);
    console_write("\r\n");
    console_write("bNumConfigurations: ");
    console_write_hex(device_descriptor->bNumConfigurations);
    console_write("\r\n");

    // Get the configuration descriptor.
    // bmRequestType = 0x80 (Dir = Device to Host, Type = Standard, Recipient = Device)
    // bRequest = GET_DESCRIPTOR (0x6)
    // wValue = Configuration Descriptor (0x2)
    setup_packet_trb.parameter_low = 0x80 | (0x6 << 8) | (0x2 << 24);
    // wIndex = 0 (no index)
    // wLength = 0x9 (8 bytes)
    setup_packet_trb.parameter_high = (0x9 << 16);
    setup_packet_trb.status = 0x8;
    setup_packet_trb.control = (1 << 6) | (0x3 << 16) | XHCI_TRB_TYPE_SETUP_STAGE;
    xhci_command_ring_enqueue(&ep0_transfer_ring, &setup_packet_trb);

    // Clear the data stage buffer.
    memory_set((void *)data_stage_buffer, 0, 4096);
    
    data_stage_trb.parameter_low = (uint32_t)data_stage_buffer;
    data_stage_trb.parameter_high = (uint32_t)(data_stage_buffer >> 32);
    data_stage_trb.status = 0x9; // Transfer Size = 0x8 (8 bytes)
    data_stage_trb.control = (1 << 16) | XHCI_TRB_TYPE_DATA_STAGE; // DIR = 1, IDT = 0, IOC = 0, CH = 0
    xhci_command_ring_enqueue(&ep0_transfer_ring, &data_stage_trb);

    status_stage_trb.parameter_low = 0;
    status_stage_trb.parameter_high = 0;
    status_stage_trb.status = 0x0;
    status_stage_trb.control = (1 << 5) | XHCI_TRB_TYPE_STATUS_STAGE; // IOC = 1, 
    xhci_command_ring_enqueue(&ep0_transfer_ring, &status_stage_trb);

    // Ring the transfer ring doorbell.
    xhci_ring_doorbell(slot_id, 0x1); // 0x1 = EP0 Enqueue Pointer Update

    xhci_event_ring_wait_for_event(&event_ring);
    console_write("Event ring has a valid TRB\r\n");

    xhci_event_ring_dequeue(&event_ring, &event_trb);

    if (!xhci_trb_is_type(&event_trb, XHCI_TRB_TYPE_TRANSFER_EVENT)) {
        console_write("Event TRB is not a transfer event\r\n");
        return -1;
    }

    completion_code = (event_trb.status >> 24) & 0xff;
    if (completion_code != 1) {
        console_write("Transfer event failed with completion code: ");
        console_write_hex(completion_code);
        console_write("\r\n");
        return -1;
    }

    console_write("Configuration descriptor transfer completed successfully\r\n");

    usb_configuration_descriptor_t *configuration_descriptor = (usb_configuration_descriptor_t *)data_stage_buffer;
    console_write("Configuration Descriptor: \r\n");
    console_write("bLength: ");
    console_write_hex(configuration_descriptor->bLength);
    console_write("\r\n");
    console_write("bDescriptorType: ");
    console_write_hex(configuration_descriptor->bDescriptorType);
    console_write("\r\n");
    console_write("wTotalLength: ");
    console_write_hex(configuration_descriptor->wTotalLength);
    console_write("\r\n");
    console_write("bNumInterfaces: ");
    console_write_hex(configuration_descriptor->bNumInterfaces);
    console_write("\r\n");
    console_write("bConfigurationValue: ");
    console_write_hex(configuration_descriptor->bConfigurationValue);
    console_write("\r\n");
    console_write("iConfiguration: ");
    console_write_hex(configuration_descriptor->iConfiguration);
    console_write("\r\n");
    console_write("bmAttributes: ");
    console_write_hex(configuration_descriptor->bmAttributes);
    console_write("\r\n");
    console_write("bMaxPower: ");
    console_write_hex(configuration_descriptor->bMaxPower);
    console_write("\r\n");

    // Fetch all of the configuration descriptors.

    // bmRequestType = 0x80 (Dir = Device to Host, Type = Standard, Recipient = Device)
    // bRequest = GET_DESCRIPTOR (0x6)
    // wValue = Configuration Descriptor (0x2)
    setup_packet_trb.parameter_low = 0x80 | (0x6 << 8) | (0x2 << 24);
    // wIndex = 0 (no index)
    // wLength = wTotalLength
    setup_packet_trb.parameter_high = (configuration_descriptor->wTotalLength << 16);
    setup_packet_trb.status = 0x8;
    setup_packet_trb.control = (1 << 6) | (0x3 << 16) | XHCI_TRB_TYPE_SETUP_STAGE;
    xhci_command_ring_enqueue(&ep0_transfer_ring, &setup_packet_trb);

    data_stage_trb.parameter_low = (uint32_t)data_stage_buffer;
    data_stage_trb.parameter_high = (uint32_t)(data_stage_buffer >> 32);
    data_stage_trb.status = configuration_descriptor->wTotalLength; // Transfer Size = wTotalLength
    data_stage_trb.control = (1 << 16) | XHCI_TRB_TYPE_DATA_STAGE; // DIR = 1, IDT = 0, IOC = 0, CH = 0
    xhci_command_ring_enqueue(&ep0_transfer_ring, &data_stage_trb);

    status_stage_trb.parameter_low = 0;
    status_stage_trb.parameter_high = 0;
    status_stage_trb.status = 0x0;
    status_stage_trb.control = (1 << 5) | XHCI_TRB_TYPE_STATUS_STAGE; // IOC = 1, 
    xhci_command_ring_enqueue(&ep0_transfer_ring, &status_stage_trb);

    // Ring the transfer ring doorbell.
    xhci_ring_doorbell(slot_id, 0x1); // 0x1 = EP0 Enqueue Pointer Update

    xhci_event_ring_wait_for_event(&event_ring);
    console_write("Event ring has a valid TRB\r\n");

    xhci_event_ring_dequeue(&event_ring, &event_trb);

    if (!xhci_trb_is_type(&event_trb, XHCI_TRB_TYPE_TRANSFER_EVENT)) {
        console_write("Event TRB is not a transfer event\r\n");
        return -1;
    }

    completion_code = (event_trb.status >> 24) & 0xff;
    if (completion_code != 1) {
        console_write("Transfer event failed with completion code: ");
        console_write_hex(completion_code);
        console_write("\r\n");
        return -1;
    }

    console_write("Configuration descriptors transfer completed successfully\r\n");

    usb_interface_descriptor_t *interface_descriptor = NULL;
    usb_endpoint_descriptor_t *endpoint_descriptor = NULL;

    // Iterate through the configuration descriptors.
    uint8_t *configuration_descriptors = (uint8_t *)data_stage_buffer;
    uint8_t b_length = 0;
    uint8_t b_descriptor_type = 0;
    uint64_t offset = 0;
    while (offset < configuration_descriptor->wTotalLength) {
        b_length = configuration_descriptors[offset];
        b_descriptor_type = configuration_descriptors[offset + 1];

        console_write("Descriptor Type: ");
        console_write_hex(b_descriptor_type);
        console_write("\r\n");

        // Configuration descriptor.
        if (b_descriptor_type == 0x2) {

        }

        // Interface descriptor.
        if (b_descriptor_type == 0x4) {
            interface_descriptor = (usb_interface_descriptor_t *)(configuration_descriptors + offset);
            console_write("Interface Descriptor: \r\n");
            console_write("bLength: ");
            console_write_hex(interface_descriptor->bLength);
            console_write("\r\n");
            console_write("bDescriptorType: ");
            console_write_hex(interface_descriptor->bDescriptorType);
            console_write("\r\n");
            console_write("bInterfaceNumber: ");
            console_write_hex(interface_descriptor->bInterfaceNumber);
            console_write("\r\n");
            console_write("bAlternateSetting: ");
            console_write_hex(interface_descriptor->bAlternateSetting);
            console_write("\r\n");
            console_write("bNumEndpoints: ");
            console_write_hex(interface_descriptor->bNumEndpoints);
            console_write("\r\n");
            console_write("bInterfaceClass: ");
            console_write_hex(interface_descriptor->bInterfaceClass);
            console_write("\r\n");
            console_write("bInterfaceSubClass: ");
            console_write_hex(interface_descriptor->bInterfaceSubClass);
            console_write("\r\n");
            console_write("bInterfaceProtocol: ");
            console_write_hex(interface_descriptor->bInterfaceProtocol);
            console_write("\r\n");
            console_write("iInterface: ");
            console_write_hex(interface_descriptor->iInterface);
            console_write("\r\n");
        }

        // Endpoint descriptor.
        if (b_descriptor_type == 0x5) {
            endpoint_descriptor = (usb_endpoint_descriptor_t *)(configuration_descriptors + offset);
            console_write("Endpoint Descriptor: \r\n");
            console_write("bLength: ");
            console_write_hex(endpoint_descriptor->bLength);
            console_write("\r\n");
            console_write("bDescriptorType: ");
            console_write_hex(endpoint_descriptor->bDescriptorType);
            console_write("\r\n");
            console_write("bEndpointAddress: ");
            console_write_hex(endpoint_descriptor->bEndpointAddress);
            console_write("\r\n");
            console_write("bmAttributes: ");
            console_write_hex(endpoint_descriptor->bmAttributes);
            console_write("\r\n");
            console_write("wMaxPacketSize: ");
            console_write_hex(endpoint_descriptor->wMaxPacketSize);
            console_write("\r\n");
            console_write("bInterval: ");
            console_write_hex(endpoint_descriptor->bInterval);
            console_write("\r\n");
        }
        
        offset += b_length;
    }

    // Send SET CONFIGURATION command.

    // bmRequestType = 0x0
    // bRequest = SET_CONFIGURATION (0x9)
    // wValue = Configuration Value (bConfigurationValue)
    setup_packet_trb.parameter_low = (0x9 << 8) | (configuration_descriptor->bConfigurationValue << 16);
    // wIndex = 0 (no index)
    // wLength = 0
    setup_packet_trb.parameter_high = 0;
    setup_packet_trb.status = 0x8;
    setup_packet_trb.control = (1 << 6) | XHCI_TRB_TYPE_SETUP_STAGE;
    xhci_command_ring_enqueue(&ep0_transfer_ring, &setup_packet_trb);

    status_stage_trb.parameter_low = 0;
    status_stage_trb.parameter_high = 0;
    status_stage_trb.status = 0x0;
    status_stage_trb.control = (1 << 5) | (1 << 16) | XHCI_TRB_TYPE_STATUS_STAGE; // IOC = 1, 
    xhci_command_ring_enqueue(&ep0_transfer_ring, &status_stage_trb);

    // Ring the transfer ring doorbell.
    xhci_ring_doorbell(slot_id, 0x1); // 0x1 = EP0 Enqueue Pointer Update

    xhci_event_ring_wait_for_event(&event_ring);
    console_write("Event ring has a valid TRB\r\n");

    xhci_event_ring_dequeue(&event_ring, &event_trb);

    if (!xhci_trb_is_type(&event_trb, XHCI_TRB_TYPE_TRANSFER_EVENT)) {
        console_write("Event TRB is not a transfer event\r\n");
        return -1;
    }

    completion_code = (event_trb.status >> 24) & 0xff;
    if (completion_code != 1) {
        console_write("Transfer event failed with completion code: ");
        console_write_hex(completion_code);
        console_write("\r\n");
        return -1;
    }

    console_write("SET CONFIGURATION command completed successfully\r\n");

    // TODO: Enable the endpoint at address endpoint_descriptor->bEndpointAddress (0x81 = Address 0x1 and Direction = IN).
    uint8_t endpoint_address = endpoint_descriptor->bEndpointAddress & 0x7f;
    uint32_t endpoint_context_index = 0;
    if ((endpoint_descriptor->bEndpointAddress & (1 << 7)) > 0) {
        // IN endpoint.
        endpoint_context_index = 2 * (uint32_t)endpoint_address + 1;
    } else {
        // OUT endpoint.
        endpoint_context_index = 2 * (uint32_t)endpoint_address;
    }

    xhci_input_context_t *input_device_context = (xhci_input_context_t *)(input_context_base);
    input_device_context->drop_context_flags = 0;
    input_device_context->add_context_flags = (1 << 0) | (1 << endpoint_context_index); // A0 (Slot Context Update), A3 (Endpoint Context 1 IN Update)

    xhci_slot_context_t *slot_context = (xhci_slot_context_t *)(input_context_base + sizeof(xhci_input_context_t));
    SLOT_CONTEXT_SET_CONTEXT_ENTRIES(slot_context, endpoint_context_index);

    // Create EP Context 1 IN.
    uint64_t endpoint_context_1_transfer_ring_base = 0;
    if (page_alloc_block(PAGE_ALLOC_ORDER_4KB, &endpoint_context_1_transfer_ring_base) < 0) {
        console_write("Failed to allocate endpoint context 1 transfer ring base\r\n");
        return -1;
    }
    memory_set((void *)endpoint_context_1_transfer_ring_base, 0, 4096);

    xhci_trb_ring_t endpoint_context_1_transfer_ring;
    xhci_command_ring_init(&endpoint_context_1_transfer_ring, endpoint_context_1_transfer_ring_base, 256);

    xhci_endpoint_context_t *input_endpoint_context_1_in = (xhci_endpoint_context_t *)((uint64_t)input_slot_context + endpoint_context_index * sizeof(xhci_endpoint_context_t));
    ENDPOINT_CONTEXT_SET_EP_TYPE(input_endpoint_context_1_in, 0x7); // Interrupt IN Endpoint (0x7)
    ENDPOINT_CONTEXT_SET_CErr(input_endpoint_context_1_in, 0x3); // 3 retries on error.
    input_endpoint_context_1_in->max_packet_size = endpoint_descriptor->wMaxPacketSize & 0x7ff;
    input_endpoint_context_1_in->interval = endpoint_descriptor->bInterval;
    input_endpoint_context_1_in->transfer_ring_dequeue_ptr = endpoint_context_1_transfer_ring_base | 0x1; // Set the cycle bit.
    input_endpoint_context_1_in->average_trb_length = 0x4; // 4 bytes for reports from the mouse.
    input_endpoint_context_1_in->max_esit_payload_low = input_endpoint_context_1_in->max_packet_size;

    xhci_trb_t configure_endpoint_trb = {
        .parameter_low = (uint32_t)input_context_base,
        .parameter_high = (uint32_t)(input_context_base >> 32),
        .status = 0,
        .control = XHCI_TRB_TYPE_CONFIGURE_ENDPOINT_COMMAND | (slot_id << 24)
    };
    xhci_command_ring_enqueue(&command_ring, &configure_endpoint_trb);

    // Enqueue a LINK command TRB -- Links back to the start of the same ring, toggles the cycle bit.
    xhci_command_ring_enqueue(&command_ring, &link_command_trb);

    // Ring the command ring doorbell.
    xhci_ring_command_doorbell();

    // Wait for an event to be available on the event ring.
    xhci_event_ring_wait_for_event(&event_ring);
    console_write("Event ring has a valid TRB\r\n");

    xhci_event_ring_dequeue(&event_ring, &event_trb);

    if (!xhci_trb_is_type(&event_trb, XHCI_TRB_TYPE_COMMAND_COMPLETION)) {
        console_write("Event TRB is not a command completion\r\n");
        return -1;
    }

    completion_code = (event_trb.status >> 24) & 0xff;
    if (completion_code != 1) {
        console_write("Command failed with completion code: ");
        console_write_hex(completion_code);
        console_write("\r\n");
        return -1;
    }

    console_write("Configure endpoint command completed successfully\r\n");

    // Output Endpoint Context 1 IN.
    xhci_endpoint_context_t *output_endpoint_context_1_in = (xhci_endpoint_context_t *)(dcba_table[slot_id] + endpoint_context_index * sizeof(xhci_endpoint_context_t));
    console_write("Output Endpoint Context 1 IN: \r\n");
    console_write("Endpoint State: ");
    console_write_hex(output_endpoint_context_1_in->endpoint_state);
    console_write("\r\n");
    console_write("LSA Max P Streams Mult: ");
    console_write_hex(output_endpoint_context_1_in->lsa_max_p_streams_mult);
    console_write("\r\n");
    console_write("Interval: ");
    console_write_hex(output_endpoint_context_1_in->interval);
    console_write("\r\n");
    console_write("Max ESIT Payload High: ");
    console_write_hex(output_endpoint_context_1_in->max_esit_payload_high);
    console_write("\r\n");
    console_write("Control: ");
    console_write_hex(output_endpoint_context_1_in->control);
    console_write("\r\n");
    console_write("Max Burst Size: ");
    console_write_hex(output_endpoint_context_1_in->max_burst_size);
    console_write("\r\n");
    console_write("Max Packet Size: ");
    console_write_hex(output_endpoint_context_1_in->max_packet_size);
    console_write("\r\n");
    console_write("Transfer Ring Dequeue Ptr: ");
    console_write_hex(output_endpoint_context_1_in->transfer_ring_dequeue_ptr);
    console_write("\r\n");
    console_write("Average TRB Length: ");
    console_write_hex(output_endpoint_context_1_in->average_trb_length);
    console_write("\r\n");
    console_write("Max ESIT Payload Low: ");
    console_write_hex(output_endpoint_context_1_in->max_esit_payload_low);
    console_write("\r\n");

    while (1) {
        memory_set((void *)data_stage_buffer, 0, 4096);

        // Enqueue a Normal TRB for the endpoint to fetch data from the device.
        xhci_trb_t mouse_transfer_trb = {
            .parameter_low = (uint32_t)data_stage_buffer,
            .parameter_high = (uint32_t)(data_stage_buffer >> 32),
            .status = 0x4, // Transfer Size = 0x4 (4 bytes)
            .control = XHCI_TRB_TYPE_NORMAL | (1 << 5) | (1 << 2) // IOC = 1.
        };
        xhci_command_ring_enqueue(&endpoint_context_1_transfer_ring, &mouse_transfer_trb);

        if (endpoint_context_1_transfer_ring.entry_count >= endpoint_context_1_transfer_ring.ring_size - 1) {
            xhci_trb_t mouse_link_trb = {
                .parameter_low = (uint32_t)endpoint_context_1_transfer_ring_base,
                .parameter_high = (uint32_t)(endpoint_context_1_transfer_ring_base >> 32),
                .status = 0,
                .control = XHCI_TRB_TYPE_LINK | XHCI_TRB_LINK_TOGGLE_CYLCE_BIT
            };
            xhci_command_ring_enqueue(&endpoint_context_1_transfer_ring, &mouse_link_trb);
        }

        xhci_ring_doorbell(slot_id, endpoint_context_index);

        // Wait for an event to be available on the event ring.
        xhci_event_ring_wait_for_event(&event_ring);

        xhci_event_ring_dequeue(&event_ring, &event_trb);

        if (!xhci_trb_is_type(&event_trb, XHCI_TRB_TYPE_TRANSFER_EVENT)) {
            console_write("Event TRB is not a transfer event\r\n");
            return -1;
        }

        completion_code = (event_trb.status >> 24) & 0xff;
        if (completion_code != 1) {
            console_write("Transfer event failed with completion code: ");
            console_write_hex(completion_code);
            console_write("\r\n");
            return -1;
        }

        typedef struct mouse_report_t {
            uint8_t buttons;
            int8_t x;
            int8_t y;
        } mouse_report_t;

        mouse_report_t *mouse_report = (mouse_report_t *)data_stage_buffer;
        console_write("Buttons: ");
        console_write_hex(mouse_report->buttons);
        console_write("\r\n");
        console_write("X: ");
        console_write_hex(mouse_report->x);
        console_write("\r\n");
        console_write("Y: ");
        console_write_hex(mouse_report->y);
        console_write("\r\n");
    }

    return 0;
}
