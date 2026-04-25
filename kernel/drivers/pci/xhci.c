#include "xhci.h"
#include "pcie.h"
#include "../../memory.h"
#include "../../console.h"
#include "../../format.h"
#include "../../page_alloc.h"
#include "../../mmap.h"
#include "../../utils/ring_buffer.h"
#include "../usb/usb_core.h"

#include <stdint.h>
#include <stddef.h>

// Where to map the XHCI heap (general purpose memory mapped as normal non-cached memory).
#define XHCI_HEAP_VIRTUAL_BASE 0xffffffff80000000ULL
#define XHCI_HEAP_SIZE (1ULL << 21) // 2MiB
#define XHCI_HEAP_MAX_PAGES (XHCI_HEAP_SIZE / PAGE_SIZE)

#define XHCI_MAX_NUM_SLOTS (1 << 8)
#define XHCI_MAX_NUM_ENDPOINTS (1 << 5)
#define XHCI_MAX_NUM_TRANSFER_JOBS (1 << 8)

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

// PORTSC bits.
#define XHCI_PORTSC_CONNECT_STATUS(portsc) (portsc & 0x1)
#define XHCI_PORTSC_PORT_ENABLED(portsc) (portsc & 0x2)

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

// Setup Stage TRB control field bits.
#define XHCI_TRB_CONTROL_ISP          (1 << 2)
#define XHCI_TRB_CONTROL_IOC          (1 << 5)
#define XHCI_TRB_CONTROL_IDT          (1 << 6)
#define XHCI_TRB_CONTROL_TRT(x)       (((x) & 0x3) << 16)
#define XHCI_TRB_CONTROL_TRT_NO_DATA  XHCI_TRB_CONTROL_TRT(0x0)
#define XHCI_TRB_CONTROL_TRT_DATA_OUT XHCI_TRB_CONTROL_TRT(0x2)
#define XHCI_TRB_CONTROL_TRT_DATA_IN  XHCI_TRB_CONTROL_TRT(0x3)

// Data Stage TRB control field bits.
#define XHCI_TRB_DATA_STAGE_IN_DIRECTION (1 << 16)

#define XHCI_COMPLETION_CODE_SUCCESS 0x1

// TRB Control bits
#define XHCI_TRB_CONTROL_CYCLE_BIT (1 << 0)
#define XHCI_TRB_LINK_TOGGLE_CYLCE_BIT (1 << 1)

// Doorbell offsets.
#define XHCI_DOORBELL_CONTROLLER 0x0

// Endpoint types.
#define XHCI_ENDPOINT_TYPE_INVALID 0x0
#define XHCI_ENDPOINT_TYPE_OUT_ISOCHRONOUS 0x1
#define XHCI_ENDPOINT_TYPE_OUT_BULK 0x2
#define XHCI_ENDPOINT_TYPE_OUT_INTERRUPT 0x3
#define XHCI_ENDPOINT_TYPE_CONTROL 0x4
#define XHCI_ENDPOINT_TYPE_IN_ISOCHRONOUS 0x5
#define XHCI_ENDPOINT_TYPE_IN_BULK 0x6
#define XHCI_ENDPOINT_TYPE_IN_INTERRUPT 0x7

typedef struct xhci_trb_t {
    uint32_t parameter_low;
    uint32_t parameter_high;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

#define XHCI_EVENT_TRB_COMPLETION_CODE(trb) ((trb)->status >> 24) & 0xff
#define XHCI_EVENT_TRB_SLOT_ID(trb) ((trb)->control >> 24) & 0xff
#define XHCI_EVENT_TRB_ENDPOINT_ID(trb) ((trb)->control >> 16) & 0x1f
#define XHCI_EVENT_TRB_TYPE(trb) ((trb)->control >> 10) & 0x3f
#define XHCI_EVENT_TRB_TRANSFER_LENGTH(trb) ((trb)->status & 0xffffff)

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

#define CLEAR_AND_SET(value, mask, set) value = ((value) & ~(mask)) | (set & (mask))
#define BIT_MASK(bits) ((1 << bits) - 1)

// Helpers for setting the bitfields in the slot context.
#define SLOT_CONTEXT_SET_CONTEXT_ENTRIES(context, entries)   CLEAR_AND_SET((context)->data0, BIT_MASK(5) << 26, (entries) << 27)
#define SLOT_CONTEXT_SET_HUB(context)                        CLEAR_AND_SET((context)->data0, BIT_MASK(1) << 26, 1 << 26)
#define SLOT_CONTEXT_SET_MTT(context)                        CLEAR_AND_SET((context)->data0, BIT_MASK(1) << 25, 1 << 25)
#define SLOT_CONTEXT_SET_SPEED(context, speed)               CLEAR_AND_SET((context)->data0, BIT_MASK(4) << 20, (speed) << 20)
#define SLOT_CONTEXT_SET_ROUTE_STRING(context, route_string) CLEAR_AND_SET((context)->data0, BIT_MASK(20), route_string)
#define SLOT_CONTEXT_SET_INTERRUPTER_TARGET(context, target) CLEAR_AND_SET((context)->data1, BIT_MASK(10) << 6, ((target) << 6))
#define SLOT_CONTEXT_SET_TTT(context, ttt)                   CLEAR_AND_SET((context)->data1, BIT_MASK(2) << 0, ((ttt) << 0))
#define SLOT_CONTEXT_SET_SLOT_STATE(context, state)          CLEAR_AND_SET((context)->data2, BIT_MASK(5) << 3, (state) << 3)

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

// Helpers for setting the bitfields in the endpoint context.
#define ENDPOINT_CONTEXT_SET_HID(context, hid)      CLEAR_AND_SET((context)->control, BIT_MASK(1) << 7, (hid) << 7)
#define ENDPOINT_CONTEXT_SET_EP_TYPE(context, type) CLEAR_AND_SET((context)->control, BIT_MASK(3) << 3, (type) << 3)
#define ENDPOINT_CONTEXT_SET_CErr(context, c_err)   CLEAR_AND_SET((context)->control, BIT_MASK(2) << 1, (c_err) << 1)

typedef struct xhci_transfer_job_t {
    xhci_transfer_callback_t callback;
    void *callback_context;
    xhci_allocated_page_t data_buffer;
    uint64_t transfer_length;
    uint8_t has_data;
} xhci_transfer_job_t;

typedef struct xhci_endpoint_state_t {
    xhci_transfer_job_t transfer_job_base[XHCI_MAX_NUM_TRANSFER_JOBS];
    ring_buffer_t transfer_jobs;
} xhci_endpoint_state_t;

typedef struct xhci_device_state_t {
    uint8_t enabled;
    xhci_endpoint_state_t endpoints[XHCI_MAX_NUM_ENDPOINTS];
    uint8_t num_configured_endpoints;
} xhci_device_state_t;

typedef struct xhci_driver_t {
    volatile uint8_t *mmio_base;         // Base address of the MMIO registers.
    volatile uint8_t *caps;              // Base address of the capabilities registers.
    uint8_t cap_length;                  // Length of the capabilities registers.
    volatile uint8_t *ops;               // Base address of the operation registers.
    volatile uint8_t *rt;                // Base address of the runtime registers.
    volatile uint32_t *doorbells_array;  // Base address of the doorbell registers (Doorbell Array)
    volatile uint64_t *dcba_array;       // Base address of the DCBA table.

    // Track which pages in the XHCI heap have been allocated.
    uint8_t heap_page_allocated[XHCI_HEAP_MAX_PAGES];
    uint64_t last_modified_heap_page_idx;

    // PCIe bus, device, and function (BDF) numbers of the xHCI controller.
    uint32_t pci_bus;
    uint32_t pci_device;
    uint32_t pci_function;

    xhci_trb_ring_t command_ring;
    xhci_trb_ring_t event_ring;
    xhci_event_ring_segment_table_t *event_ring_segment_table;

    // Indexed by slot ID.
    xhci_device_state_t devices[XHCI_MAX_NUM_SLOTS];
} xhci_driver_t;

static xhci_driver_t g_xhci_driver = {
    .mmio_base = NULL,
    .caps = NULL,
    .ops = NULL,
    .rt = NULL,
    .doorbells_array = NULL,
    .pci_bus = 0,
    .pci_device = 0,
    .pci_function = 0,
    .dcba_array = NULL,
    .command_ring = {
        .base_address = NULL,
        .enqueue_ptr = NULL,
        .dequeue_ptr = NULL,
        .ring_size = 0,
        .entry_count = 0,
        .cycle_bit = 0,
        .link_trb = NULL,
    },
    .event_ring = {
        .base_address = NULL,
        .enqueue_ptr = NULL,
        .dequeue_ptr = NULL,
        .ring_size = 0,
        .entry_count = 0,
        .cycle_bit = 0,
        .link_trb = NULL,
    },
    .event_ring_segment_table = NULL,
    .last_modified_heap_page_idx = 0,
};

static inline void xhci_data_sync_barrier(void) {
    __asm__ volatile("dsb sy" ::: "memory");
}

uint32_t xhci_read_op_register(uint32_t offset) {
    return *(volatile uint32_t*)(g_xhci_driver.ops + offset);
}

void xhci_write_op_register(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(g_xhci_driver.ops + offset) = value;
}

uint64_t xhci_read_op_register64(uint32_t offset) {
    return *(volatile uint64_t*)(g_xhci_driver.ops + offset);
}

void xhci_write_op_register64(uint32_t offset, uint64_t value) {
    *(volatile uint64_t*)(g_xhci_driver.ops + offset) = value;
}

uint32_t xhci_read_cap_register(uint32_t offset) {
    return *(volatile uint32_t*)(g_xhci_driver.caps + offset);
}

void xhci_write_cap_register(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(g_xhci_driver.caps + offset) = value;
}

uint64_t xhci_read_cap_register64(uint32_t offset) {
    return *(volatile uint64_t*)(g_xhci_driver.caps + offset);
}

void xhci_write_cap_register64(uint32_t offset, uint64_t value) {
    *(volatile uint64_t*)(g_xhci_driver.caps + offset) = value;
}

uint32_t xhci_read_rt_register(uint32_t offset) {
    return *(volatile uint32_t*)(g_xhci_driver.rt + offset);
}

void xhci_write_rt_register(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(g_xhci_driver.rt + offset) = value;
}

uint64_t xhci_read_rt_register64(uint32_t offset) {
    return *(volatile uint64_t*)(g_xhci_driver.rt + offset);
}

void xhci_write_rt_register64(uint32_t offset, uint64_t value) {
    *(volatile uint64_t*)(g_xhci_driver.rt + offset) = value;
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

int xhci_alloc_page(xhci_allocated_page_t *allocated_page) {
    uint8_t checked_pages[XHCI_HEAP_MAX_PAGES];
    memory_set(checked_pages, 0, XHCI_HEAP_MAX_PAGES);

    uint64_t page_idx = g_xhci_driver.last_modified_heap_page_idx;
    uint8_t page_allocated = g_xhci_driver.heap_page_allocated[page_idx];

    while (page_allocated == 1 && checked_pages[page_idx] == 0) {
        checked_pages[page_idx] = 1;
        page_idx++;

        if (page_idx >= XHCI_HEAP_MAX_PAGES) {
            page_idx = 0;
        }

        page_allocated = g_xhci_driver.heap_page_allocated[page_idx];
    }

    // All pages are allocated, cannot allocate more pages.
    if (page_allocated == 1) {
        console_write("All pages in the XHCI heap are allocated\r\n");
        return -1;
    }

    allocated_page->virtual_address = XHCI_HEAP_VIRTUAL_BASE + (page_idx * PAGE_SIZE);
    allocated_page->physical_address = 0;

    if (mmap_virtual_to_physical(allocated_page->virtual_address, &allocated_page->physical_address) < 0) {
        console_write("Failed to map allocated page to physical address\r\n");
        return -1;
    }

    g_xhci_driver.heap_page_allocated[page_idx] = 1;
    g_xhci_driver.last_modified_heap_page_idx = page_idx;
    return 0;
}

int xhci_free_page(xhci_allocated_page_t *allocated_page) {
    uint64_t page_idx = (uint64_t)(allocated_page->virtual_address - XHCI_HEAP_VIRTUAL_BASE) / PAGE_SIZE;
    if (page_idx >= XHCI_HEAP_MAX_PAGES) {
        console_write("Invalid page index ");
        console_write_hex(page_idx);
        console_write("\r\n");
        console_write("XHCI heap max pages: ");
        console_write_hex(XHCI_HEAP_MAX_PAGES);
        console_write("\r\n");
        return -1;
    }
    g_xhci_driver.heap_page_allocated[page_idx] = 0;
    g_xhci_driver.last_modified_heap_page_idx = page_idx;

    return 0;
}

int xhci_locate_device(void) {
    int pcie_search_result = pcie_locate_device(
        0x0,
        PCI_CLASS_SERIAL_BUS_CONTROLLER,
        PCI_SUBCLASS_USB_CONTROLLER,
        PCI_PROG_INT_USB_3_0_XHCI,
        &g_xhci_driver.pci_bus,
        &g_xhci_driver.pci_device,
        &g_xhci_driver.pci_function
    );
    if (pcie_search_result != 0) {
        console_write("Failed to locate xHCI controller\r\n");
        return -1;
    }

    uint32_t bar0 = pcie_config_read32(
        g_xhci_driver.pci_bus,
        g_xhci_driver.pci_device,
        g_xhci_driver.pci_function,
        PCI_CONFIG_BAR0
    );
    uint32_t bar1 = pcie_config_read32(
        g_xhci_driver.pci_bus,
        g_xhci_driver.pci_device,
        g_xhci_driver.pci_function,
        PCI_CONFIG_BAR1
    );
    
    // TODO: Map this address space to the kernel's virtual address space.
    // TODO: Make sure to map this as device memory (not normal memory).
    uint64_t base_address_low = (uint64_t)(bar0 & 0xfffffff0);
    uint64_t base_address_high = (uint64_t)(bar1 & 0xfffffff0);
    uint64_t base_address = (base_address_high << 32) | base_address_low;

    // Enable the Bus Master and Memory Space capabilities.
    pcie_config_write16(
        g_xhci_driver.pci_bus,
        g_xhci_driver.pci_device,
        g_xhci_driver.pci_function,
        PCI_CONFIG_COMMAND,
        PCI_COMMAND_MEMORY_SPACE_ENABLE | PCI_COMMAND_BUS_MASTER_ENABLE
    );

    g_xhci_driver.mmio_base = (uint8_t *)base_address;
    g_xhci_driver.caps = g_xhci_driver.mmio_base;
    g_xhci_driver.cap_length = g_xhci_driver.mmio_base[0];
    g_xhci_driver.ops = g_xhci_driver.mmio_base + g_xhci_driver.cap_length;

    // Read the RTSOFF register to find the runtime registers base address.
    uint32_t rtsoff = xhci_read_cap_register(XHCI_CAP_RTSOFF);
    g_xhci_driver.rt = g_xhci_driver.caps + rtsoff;

    // Read the DBOFF register to find the doorbell registers base address.
    uint32_t dboff = xhci_read_cap_register(XHCI_CAP_DBOFF);
    g_xhci_driver.doorbells_array = (uint32_t *)(g_xhci_driver.caps + dboff);

    return 0;
}

int xhci_transfer_ring_init(xhci_trb_ring_t *transfer_ring, xhci_allocated_page_t *base_address, uint64_t ring_size) {
    transfer_ring->base_address = (xhci_trb_t*)base_address->virtual_address;
    transfer_ring->enqueue_ptr = transfer_ring->base_address;
    transfer_ring->ring_size = ring_size;
    transfer_ring->entry_count = 0;
    transfer_ring->cycle_bit = 1;

    // Write the link TRB in the last position of the ring.
    xhci_trb_t link_trb = {
        .parameter_low = (uint32_t)base_address->physical_address,
        .parameter_high = (uint32_t)(base_address->physical_address >> 32),
        .status = 0,
        .control = XHCI_TRB_TYPE_LINK | XHCI_TRB_LINK_TOGGLE_CYLCE_BIT | 0x1, // Set the cycle bit.
    };
    transfer_ring->link_trb = transfer_ring->base_address + (transfer_ring->ring_size - 1);
    *transfer_ring->link_trb = link_trb;

    return 0;
}

int xhci_transfer_ring_enqueue(xhci_trb_ring_t *transfer_ring, xhci_trb_t *trb) {
    if (transfer_ring->entry_count >= transfer_ring->ring_size - 1) {
        // Make sure the link TRB cycle bit is valid so it gets processed correctly.
        if (transfer_ring->cycle_bit == 1) {
            transfer_ring->link_trb->control |= 1;
        } else {
            transfer_ring->link_trb->control &= ~1;
        }

        // Ring is full, flip the cycle bit and reset the enqueue pointer.
        transfer_ring->cycle_bit = !transfer_ring->cycle_bit;
        transfer_ring->enqueue_ptr = transfer_ring->base_address;
        transfer_ring->entry_count = 0;
    }

    // Set the cycle bit in the control field of the TRB.
    if (transfer_ring->cycle_bit == 1) {
        trb->control |= 1;
    } else {
        trb->control &= ~1;
    }

    *transfer_ring->enqueue_ptr = *trb;
    transfer_ring->enqueue_ptr++;
    transfer_ring->entry_count++;

    return 0;
}

void xhci_ring_command_doorbell(void) {
    xhci_data_sync_barrier();
    g_xhci_driver.doorbells_array[XHCI_DOORBELL_CONTROLLER] = 0;
}

void xhci_ring_doorbell(uint32_t doorbell_idx, uint32_t target) {
    xhci_data_sync_barrier();
    g_xhci_driver.doorbells_array[doorbell_idx] = target;
}

int xhci_event_ring_init(xhci_trb_ring_t *event_ring, xhci_allocated_page_t *base_address, uint64_t ring_size) {
    event_ring->base_address = (xhci_trb_t*)base_address->virtual_address;
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
    uint64_t physical_dequeue_ptr = 0;
    if (mmap_virtual_to_physical((uint64_t)event_ring->dequeue_ptr, &physical_dequeue_ptr) < 0) {
        console_write("Failed to map dequeue pointer to physical address\r\n");
        return -1;
    }
    xhci_write_rt_register64(XHCI_RT_ERDP(0), physical_dequeue_ptr | (1 << 3));

    return 0;
}

void xhci_event_ring_wait_for_event(xhci_trb_ring_t *event_ring) {
    // Spin until the event ring has a valid TRB.
    while (!xhci_event_ring_has_valid_trb(event_ring)) {}
}

int xhci_trb_is_type(xhci_trb_t *trb, uint32_t type_mask) {
    return ((trb->control >> 10) & 0x3f) == (type_mask >> 10);
}

void xhci_reset_controller(void) {
    uint32_t usb_cmd = xhci_read_op_register(XHCI_OP_USBCMD);
    xhci_write_op_register(XHCI_OP_USBCMD, usb_cmd | XHCI_OP_USBSTS_HCRST);
    xhci_wait_for_reset();
}

void xhci_set_max_device_slots_enabled(uint8_t max_device_slots) {
    uint32_t config = xhci_read_op_register(XHCI_OP_CONFIG);
    xhci_write_op_register(XHCI_OP_CONFIG, config | (max_device_slots & 0xff));
}

uint8_t xhci_get_max_device_slots(void) {
    uint32_t config = xhci_read_cap_register(XHCI_CAP_HCSPARAMS1);
    return config & 0xff;
}

uint8_t xhci_get_max_ports(void) {
    uint32_t config = xhci_read_cap_register(XHCI_CAP_HCSPARAMS1);
    return (config >> 24) & 0xff;
}

void xhci_enable_controller(void) {
    uint32_t usb_cmd = xhci_read_op_register(XHCI_OP_USBCMD);
    xhci_write_op_register(XHCI_OP_USBCMD, usb_cmd | XHCI_OP_USBCMD_RUN_STOP);
}

int xhci_allocate_dcba_array(void) {
    // Allocate a DCBA and write the address to the DCBAAP register.
    xhci_allocated_page_t dcba;
    if (xhci_alloc_page(&dcba) < 0) {
        console_write("Failed to allocate DCBA\r\n");
        return -1;
    }
    if (dcba.physical_address % (1 << 6) != 0) {
        console_write("Allocated DCBA is not aligned to 64 bytes\r\n");
        return -1;
    }

    // Zero out the DCBA.
    memory_set((void *)dcba.virtual_address, 0, PAGE_SIZE);

    // Write the DCBA address to the DCBAAP register.
    xhci_write_op_register64(XHCI_OP_DCBAAP, dcba.physical_address);

    g_xhci_driver.dcba_array = (uint64_t *)dcba.virtual_address;
    return 0;
}

int xhci_allocate_command_ring(void) {
    // Allocate a command ring base.
    xhci_allocated_page_t command_ring_base;
    if (xhci_alloc_page(&command_ring_base) < 0) {
        console_write("Failed to allocate command ring base\r\n");
        return -1;
    }
    if (command_ring_base.physical_address % (1 << 6) != 0) {
        console_write("Allocated command ring base is not aligned to 64 bytes\r\n");
        return -1;
    }

    // Zero out the command ring.
    memory_set((void *)command_ring_base.virtual_address, 0, PAGE_SIZE);

    // Write the command ring base to the CRCR register and set the CYCLE bit.
    xhci_write_op_register64(XHCI_OP_CRCR, command_ring_base.physical_address | 0x1);

    // Initialize the command ring.
    return xhci_transfer_ring_init(&g_xhci_driver.command_ring, &command_ring_base, 256);
}

int xhci_command_ring_enqueue(xhci_trb_t *trb) {
    return xhci_transfer_ring_enqueue(&g_xhci_driver.command_ring, trb);
}

int xhci_allocate_event_ring(void) {
    // Allocate memory for the ERST table.
    xhci_allocated_page_t event_ring_segment_table_base;
    if (xhci_alloc_page(&event_ring_segment_table_base) < 0) {
        console_write("Failed to allocate event ring base\r\n");
        return -1;
    }
    if (event_ring_segment_table_base.physical_address % (1 << 6) != 0) {
        console_write("Allocated event ring base is not aligned to 64 bytes\r\n");
        return -1;
    }

    // Zero out the ERST table.
    memory_set((void *)event_ring_segment_table_base.virtual_address, 0, PAGE_SIZE);
    g_xhci_driver.event_ring_segment_table = (xhci_event_ring_segment_table_t *)event_ring_segment_table_base.virtual_address;

    // Allocate memory for the event ring segment.
    xhci_allocated_page_t event_ring_segment_base;
    if (xhci_alloc_page(&event_ring_segment_base) < 0) {
        console_write("Failed to allocate event ring segment base\r\n");
        return -1;
    }
    if (event_ring_segment_base.physical_address % (1 << 6) != 0) {
        console_write("Allocated event ring segment base is not aligned to 64 bytes\r\n");
        return -1;
    }

    // Zero out the event ring segment.
    memory_set((void *)event_ring_segment_base.virtual_address, 0, PAGE_SIZE);

    // Create ERST table with a single entry mapping to a single event ring segment.
    xhci_event_ring_segment_table_t *erst = g_xhci_driver.event_ring_segment_table;
    erst->ring_segment_base_address = event_ring_segment_base.physical_address;
    erst->ring_segment_size = 256;

    // Initialize the event ring.
    xhci_trb_ring_t *event_ring = &g_xhci_driver.event_ring;
    xhci_event_ring_init(event_ring, &event_ring_segment_base, 256);

    // Write the ERST table size to the ERSTSZ register.
    xhci_write_rt_register(XHCI_RT_ERSTSZ(0), 1);
    
    // Write the ERDP (dequeue pointer) register to the event ring segment base address.
    xhci_write_rt_register64(XHCI_RT_ERDP(0), erst->ring_segment_base_address);

    // Write the ERST table base address to the first ERSTBA register.
    // NOTE: This write enables the event ring.
    xhci_write_rt_register64(XHCI_RT_ERSTBA(0), event_ring_segment_table_base.physical_address);

    return 0;
}

int xhci_command_is_successful(xhci_trb_t *trb) {
    uint8_t completion_code = (trb->status >> 24) & 0xff;
    if (completion_code != XHCI_COMPLETION_CODE_SUCCESS) {
        console_write("Command failed with completion code: ");
        console_write_hex(completion_code);
        console_write("\r\n");
        return 0;
    }

    return 1;
}

int xhci_enable_device_slot(uint8_t* enabled_slot_id) {
    xhci_trb_t enable_slot_command = {
        .parameter_low = 0,
        .parameter_high = 0,
        .status = 0,
        .control = XHCI_TRB_TYPE_ENABLE_SLOT_COMMAND,
    };
    xhci_command_ring_enqueue(&enable_slot_command);
    xhci_ring_command_doorbell();

    // Wait for an event to be available on the event ring.
    xhci_trb_ring_t *event_ring = &g_xhci_driver.event_ring;
    xhci_event_ring_wait_for_event(event_ring);

    // Dequeue the event.
    xhci_trb_t event_trb;
    xhci_event_ring_dequeue(event_ring, &event_trb);
    
    if (!xhci_trb_is_type(&event_trb, XHCI_TRB_TYPE_COMMAND_COMPLETION)) {
        console_write("Event TRB is not a command completion\r\n");
        return -1;
    }

    if (!xhci_command_is_successful(&event_trb)) {
        return -1;
    }

    // Extract the slot ID from the completion event TRB.
    uint8_t slot_id = XHCI_EVENT_TRB_SLOT_ID(&event_trb);
    *enabled_slot_id = slot_id;

    // Mark the device as enabled.
    xhci_device_state_t *device_state = &g_xhci_driver.devices[slot_id];
    device_state->enabled = 1;

    // Initialize the job queues for each endpoint.
    // TODO: We probably only need to do this for endpoints we configure.
    for (uint8_t i = 0; i < XHCI_MAX_NUM_ENDPOINTS; i++) {
        xhci_endpoint_state_t *endpoint_state = &device_state->endpoints[i];
        ring_buffer_init(
            &endpoint_state->transfer_jobs,
            (uint64_t)&endpoint_state->transfer_job_base,
            XHCI_MAX_NUM_TRANSFER_JOBS,
            sizeof(xhci_transfer_job_t)
        );
    }

    return 0;
}

int xhci_allocate_device_context(uint8_t slot_id) {
    uint64_t *dcba_table = (uint64_t *)g_xhci_driver.dcba_array;
    xhci_allocated_page_t dcba_page;
    if (xhci_alloc_page(&dcba_page) < 0) {
        console_write("Failed to allocate DCBA page\r\n");
        return -1;
    }
    memory_set((void *)dcba_page.virtual_address, 0, PAGE_SIZE);

    // Set the DCBA entry for the slot - must be a physical address.
    dcba_table[slot_id] = dcba_page.physical_address;

    return 0;
}

int xhci_address_device(uint8_t port_number, xhci_device_t *out_device) {
    if (out_device == NULL) {
        console_write("Output assigned slot is NULL\r\n");
        return -1;
    }

    uint8_t slot_id = 0;
    if (xhci_enable_device_slot(&slot_id) < 0) {
        return -1;
    }

    // Allocate memory for the enabled slot's device context in the DCBA table.
    if (xhci_allocate_device_context(slot_id) < 0) {
        return -1;
    }

    xhci_allocated_page_t input_context_base_page;
    if (xhci_alloc_page(&input_context_base_page) < 0) {
        console_write("Failed to allocate input context\r\n");
        return -1;
    }
    uint64_t input_context_base = input_context_base_page.virtual_address;
    memory_set((void *)input_context_base, 0, PAGE_SIZE);

    // Set A0 = "Slot Context" and A1 = "Endpoint Context 0" in the input control context.
    *((uint32_t *)input_context_base + 1) = 0x3; // 0b11

    // Input Slot Context.
    uint32_t *input_slot_context = (uint32_t *)(input_context_base + 8 * sizeof(uint32_t));
    input_slot_context[0] = 0x1 << 27; // Context Entries = 0x1, Route String = 0x0
    input_slot_context[1] = port_number << 16; // NOTE: Port number is 1-indexed.

    // Input Endpoint Context 0.
    xhci_endpoint_context_t *endpoint_context = (xhci_endpoint_context_t *)(input_context_base + 0x40);
    endpoint_context->max_packet_size = 64;
    endpoint_context->control = 0x4 << 3 | 0x3 << 1; // EP Type = "Control" (0x4), CErr = 0x3 (3 retries on error)

    xhci_allocated_page_t transfer_ring_base_page;
    if (xhci_alloc_page(&transfer_ring_base_page) < 0) {
        console_write("Failed to allocate transfer ring base\r\n");
        return -1;
    }
    if (transfer_ring_base_page.physical_address % (1 << 6) != 0) {
        console_write("Allocated transfer ring base is not aligned to 64 bytes\r\n");
        return -1;
    }
    memory_set((void *)transfer_ring_base_page.virtual_address, 0, PAGE_SIZE);

    endpoint_context->transfer_ring_dequeue_ptr = transfer_ring_base_page.physical_address | 0x1; // Set the cycle bit.

    xhci_trb_t address_device_command = {
        .parameter_low = (uint32_t)input_context_base_page.physical_address,
        .parameter_high = (uint32_t)(input_context_base_page.physical_address >> 32),
        .status = 0,
        .control = XHCI_TRB_TYPE_ADDRESS_DEVICE_COMMAND | (slot_id << 24),
    };
    xhci_command_ring_enqueue(&address_device_command);
    xhci_ring_command_doorbell();

    // Wait for an event to be available on the event ring.
    xhci_event_ring_wait_for_event(&g_xhci_driver.event_ring);

    // Dequeue the event.
    xhci_trb_t event_trb;
    xhci_event_ring_dequeue(&g_xhci_driver.event_ring, &event_trb);
    
    if (!xhci_trb_is_type(&event_trb, XHCI_TRB_TYPE_COMMAND_COMPLETION)) {
        console_write("Event TRB is not a command completion\r\n");
        return -1;
    }
    
    if (!xhci_command_is_successful(&event_trb)) {
        return -1;
    }

    console_write("Address device command completed successfully\r\n");

    out_device->slot_id = slot_id;
    out_device->port_number = port_number;
    out_device->input_context_base = input_context_base_page;
    out_device->input_context = (xhci_input_context_t *)input_context_base;

    // Initialize the EP0 transfer ring.
    return xhci_transfer_ring_init(&out_device->ep0_transfer_ring, &transfer_ring_base_page, 256);
}

int xhci_ep0_transfer_ring_enqueue(xhci_device_t *device, xhci_trb_t *trb) {
    return xhci_transfer_ring_enqueue(&device->ep0_transfer_ring, trb);
}

void xhci_ep0_ring_doorbell(xhci_device_t *device) {
    xhci_ring_doorbell(device->slot_id, 0x1); // 0x1 = EP0 Enqueue Pointer Update
}

void xhci_endpoint_ring_doorbell(xhci_device_t *device, xhci_endpoint_t *endpoint) {
    xhci_ring_doorbell(device->slot_id, endpoint->endpoint_context_idx);
}

static inline void dcache_clean_invalidate_range(uint64_t va, uint64_t size) {
    uint64_t line_size = 64; // or read CTR_EL0.DminLine
    uint64_t start = va & ~(line_size - 1);
    uint64_t end = (va + size + line_size - 1) & ~(line_size - 1);
    for (uint64_t p = start; p < end; p += line_size) {
        __asm__ volatile("dc civac, %0" :: "r"(p) : "memory");
    }
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
}

int xhci_allocate_heap(void) {
    // Zero out the heap page allocated array.
    memory_set(g_xhci_driver.heap_page_allocated, 0, XHCI_HEAP_MAX_PAGES);

    // Allocate physical pages for the XHCI heap.
    uint64_t xhci_heap_physical_address = 0;
    if (page_alloc_block(PAGE_ALLOC_ORDER_2MB, &xhci_heap_physical_address) < 0) {
        console_write("Failed to allocate physical pages for the XHCI heap\r\n");
        return -1;
    }

    dcache_clean_invalidate_range(xhci_heap_physical_address, XHCI_HEAP_SIZE);

    // Map the XHCI heap as Normal NC memory.
    int map_result = mmap_map_range_l2_block(
        XHCI_HEAP_VIRTUAL_BASE,
        XHCI_HEAP_VIRTUAL_BASE + XHCI_HEAP_SIZE,
        xhci_heap_physical_address,
        PAGE_FLAG_NORMAL_MEMORY_NC
    );
    if (map_result < 0) {
        console_write("Failed to map the XHCI heap as Normal NC memory\r\n");
        return -1;
    }

    console_write("XHCI heap mapped successfully\r\n");
    return 0;
}

int xhci_configure_endpoint(
    xhci_device_t *device,
    usb_endpoint_descriptor_t *endpoint_descriptor,
    xhci_endpoint_t *endpoint
) {
    if (device == NULL) {
        console_write("Input assigned slot is NULL\r\n");
        return -1;
    }

    if (endpoint_descriptor == NULL) {
        console_write("Input endpoint descriptor is NULL\r\n");
        return -1;
    }

    if (endpoint == NULL) {
        console_write("Input endpoint is NULL\r\n");
        return -1;
    }
    
    uint8_t endpoint_address = endpoint_descriptor->bEndpointAddress & 0x7f;
    uint32_t endpoint_context_index = 0;
    uint8_t endpoint_type = 0;
    if ((endpoint_descriptor->bEndpointAddress & (1 << 7)) > 0) {
        // IN endpoint.
        endpoint->endpoint_context_idx = 2 * (uint32_t)endpoint_address + 1;

        switch (endpoint_descriptor->bmAttributes & 0x3) {
            case 0x0:
                endpoint_type = XHCI_ENDPOINT_TYPE_CONTROL;
                break;
            case 0x1:
                endpoint_type = XHCI_ENDPOINT_TYPE_IN_ISOCHRONOUS;
                break;
            case 0x2:
                endpoint_type = XHCI_ENDPOINT_TYPE_IN_BULK;
                break;
            case 0x3:
                endpoint_type = XHCI_ENDPOINT_TYPE_IN_INTERRUPT;
                break;
            default:
                console_write("Invalid endpoint type\r\n");
                return -1;
        }
    } else {
        // OUT endpoint.
        endpoint->endpoint_context_idx = 2 * (uint32_t)endpoint_address;

        switch (endpoint_descriptor->bmAttributes & 0x3) {
            case 0x0:
                endpoint_type = XHCI_ENDPOINT_TYPE_CONTROL;
                break;
            case 0x1:
                endpoint_type = XHCI_ENDPOINT_TYPE_OUT_ISOCHRONOUS;
                break;
            case 0x2:
                endpoint_type = XHCI_ENDPOINT_TYPE_OUT_BULK;
                break;
            case 0x3:
                endpoint_type = XHCI_ENDPOINT_TYPE_OUT_INTERRUPT;
                break;
            default:
                console_write("Invalid endpoint type\r\n");
                return -1;
        } 
    }

    xhci_allocated_page_t input_context_base;
    if (xhci_alloc_page(&input_context_base) < 0) {
        console_write("Failed to allocate input context base\r\n");
        return -1;
    }
    memory_set((void *)input_context_base.virtual_address, 0, PAGE_SIZE);

    xhci_input_context_t *input_context = (xhci_input_context_t *)input_context_base.virtual_address;
    input_context->add_context_flags = 0x1 | (1 << endpoint->endpoint_context_idx); // Set Slot Context update and Endpoint Context update bits.

    xhci_slot_context_t *slot_context = (xhci_slot_context_t *)(
        input_context_base.virtual_address +
        sizeof(xhci_input_context_t)
    );
    slot_context->root_hub_port_number = device->port_number;
    SLOT_CONTEXT_SET_CONTEXT_ENTRIES(slot_context, endpoint->endpoint_context_idx);

    // Create the endpoint transfer ring.
    if (xhci_alloc_page(&endpoint->transfer_ring_base) < 0) {
        console_write("Failed to allocate endpoint context 1 transfer ring base\r\n");
        return -1;
    }
    memory_set((void *)endpoint->transfer_ring_base.virtual_address, 0, PAGE_SIZE);
    xhci_transfer_ring_init(&endpoint->transfer_ring, &endpoint->transfer_ring_base, 256);

    xhci_endpoint_context_t *input_endpoint_context = (xhci_endpoint_context_t *)(
        (uint64_t)slot_context +
        endpoint->endpoint_context_idx * sizeof(xhci_endpoint_context_t)
    );
    ENDPOINT_CONTEXT_SET_EP_TYPE(input_endpoint_context, endpoint_type);
    ENDPOINT_CONTEXT_SET_CErr(input_endpoint_context, 0x3); // 3 retries on error.
    input_endpoint_context->max_packet_size = endpoint_descriptor->wMaxPacketSize & 0x7ff;
    input_endpoint_context->average_trb_length = input_endpoint_context->max_packet_size;
    input_endpoint_context->interval = endpoint_descriptor->bInterval;
    input_endpoint_context->transfer_ring_dequeue_ptr = endpoint->transfer_ring_base.physical_address | 0x1; // Set the cycle bit.
    input_endpoint_context->max_esit_payload_low = input_endpoint_context->max_packet_size;

    xhci_trb_t configure_endpoint_trb = {
        .parameter_low = (uint32_t)input_context_base.physical_address,
        .parameter_high = (uint32_t)(input_context_base.physical_address >> 32),
        .status = 0,
        .control = XHCI_TRB_TYPE_CONFIGURE_ENDPOINT_COMMAND | (device->slot_id << 24)
    };
    xhci_command_ring_enqueue(&configure_endpoint_trb);

    // Wait for an event to be available on the event ring.
    xhci_ring_command_doorbell();
    xhci_event_ring_wait_for_event(&g_xhci_driver.event_ring);

    xhci_trb_t event_trb;
    xhci_event_ring_dequeue(&g_xhci_driver.event_ring, &event_trb);

    if (!xhci_trb_is_type(&event_trb, XHCI_TRB_TYPE_COMMAND_COMPLETION)) {
        console_write("Event TRB is not a command completion\r\n");
        return -1;
    }

    if (!xhci_command_is_successful(&event_trb)) {
        return -1;
    }
    console_write("Configure endpoint command completed successfully\r\n");

    xhci_device_state_t *device_state = &g_xhci_driver.devices[device->slot_id];
    device_state->num_configured_endpoints++;

    xhci_free_page(&input_context_base);
    return 0;
}

int xhci_get_port_status(uint8_t port_number, xhci_port_status_t *port_status) {
    if (port_status == NULL) {
        console_write("Input port status is NULL\r\n");
        return -1;
    }

    if (port_number >= xhci_get_max_ports()) {
        console_write("Invalid port number\r\n");
        return -1;
    }

    uint32_t port_sc = xhci_read_port_register(port_number, XHCI_PORT_PORTSC);

    port_status->port_number = port_number;
    port_status->current_connect_status = XHCI_PORTSC_CONNECT_STATUS(port_sc);
    port_status->port_enabled = XHCI_PORTSC_PORT_ENABLED(port_sc);
    
    return 0;
}

int xhci_init(void) {
    // Zero out the device state array.
    memory_set(g_xhci_driver.devices, 0, sizeof(g_xhci_driver.devices));

    if (xhci_allocate_heap() < 0) {
        return -1;
    }

    if (xhci_locate_device() != 0) {
        return -1;
    }

    // Reset the controller
    xhci_reset_controller();
    console_write("xHCI controller reset complete\r\n");

    // TODO: Assert the controller is in the correct state.

    // Enable slots that are available.
    uint8_t max_device_slots = xhci_get_max_device_slots();
    xhci_set_max_device_slots_enabled(max_device_slots);

    // Allocate the DCBA array.
    if (xhci_allocate_dcba_array() < 0) {
        return -1;
    }

    // Initialize the command ring.
    xhci_trb_ring_t *command_ring = &g_xhci_driver.command_ring;
    if (xhci_allocate_command_ring() < 0) {
        return -1;
    }
   
    // Enqueue a NO-OP command TRB.
    xhci_trb_t no_op_command_trb = {
        .parameter_low = 0,
        .parameter_high = 0,
        .status = 0,
        .control = XHCI_TRB_TYPE_NO_OP_COMMAND,
    };
    xhci_transfer_ring_enqueue(command_ring, &no_op_command_trb);

    // Initialize the event ring.
    xhci_trb_ring_t *event_ring = &g_xhci_driver.event_ring;
    if (xhci_allocate_event_ring() < 0) {
        return -1;
    }

    // Enable the xHCI controller.
    xhci_enable_controller();

    // Wait for the controller to be ready.
    xhci_wait_for_controller_ready();
    xhci_wait_for_reset();
    console_write("xHCI controller ready\r\n");

    // Send a doorbell to the controller.
    xhci_ring_command_doorbell();

    // Wait for an event to be available on the event ring.
    console_write("Waiting for event to be available on the event ring\r\n");
    xhci_event_ring_wait_for_event(event_ring);

    // Dequeue the event.
    xhci_trb_t event_trb;
    xhci_event_ring_dequeue(event_ring, &event_trb);

    if (!xhci_trb_is_type(&event_trb, XHCI_TRB_TYPE_COMMAND_COMPLETION)) {
        console_write("Event TRB is not a command completion\r\n");
        return -1;
    }

    if (!xhci_command_is_successful(&event_trb)) {
        return -1;
    }

    console_write("No-op command completed successfully\r\n");

    return 0;
}

int xhci_poll_events(void) {
    while (xhci_event_ring_has_valid_trb(&g_xhci_driver.event_ring)) {
        xhci_trb_t event_trb;
        xhci_event_ring_dequeue(&g_xhci_driver.event_ring, &event_trb);

        uint8_t slot_id = XHCI_EVENT_TRB_SLOT_ID(&event_trb);
        uint8_t endpoint_id = XHCI_EVENT_TRB_ENDPOINT_ID(&event_trb);

        if (endpoint_id == 0) {
            console_write("Invalid Endpoint ID received for event\r\n");
            continue;
        }

        xhci_device_state_t *device_state = &g_xhci_driver.devices[slot_id];
        xhci_endpoint_state_t *endpoint_state = &device_state->endpoints[endpoint_id - 1];

        if (device_state->enabled == 0) {
            console_write("Device is not enabled for the events slot\r\n");
            continue;
        }

        if (ring_buffer_is_empty(&endpoint_state->transfer_jobs)) {
            console_write("No transfer jobs for endpoint event\r\n");
            continue;
        }

        xhci_transfer_job_t transfer_job;
        ring_buffer_dequeue(&endpoint_state->transfer_jobs, &transfer_job, sizeof(xhci_transfer_job_t));

        uint8_t completion_code = XHCI_EVENT_TRB_COMPLETION_CODE(&event_trb);
        if (completion_code != XHCI_COMPLETION_CODE_SUCCESS) {
            console_write("Transfer failed with completion code: ");
            console_write_hex(completion_code);
            console_write("\r\n");

            if (transfer_job.has_data) {
                xhci_free_page(&transfer_job.data_buffer);
            }

            if (transfer_job.callback) {
                transfer_job.callback(XHCI_COMPLETION_CODE_FAILED, NULL, 0, transfer_job.callback_context);
            }
            continue;
        }

        if (transfer_job.has_data) {
            // Call the callback with the data.
            uint8_t *data = (uint8_t *)transfer_job.data_buffer.virtual_address;
            transfer_job.callback(XHCI_COMPLETION_CODE_SUCCESS, data, transfer_job.transfer_length, transfer_job.callback_context);

            // Free the data buffer.
            xhci_free_page(&transfer_job.data_buffer);
        } else {
            transfer_job.callback(XHCI_COMPLETION_CODE_SUCCESS, NULL, 0, transfer_job.callback_context);
        }
    }

    return 0;
}

int xhci_control_transfer(xhci_device_t *device, xhci_control_transfer_request_t *request) {
    xhci_transfer_job_t transfer_job;
    memory_set(&transfer_job, 0, sizeof(xhci_transfer_job_t));

    xhci_trb_t setup_packet_trb = {
        .parameter_low = (
            request->setup_packet->bmRequestType |
            (request->setup_packet->bRequest << 8) |
            (request->setup_packet->wValue << 16)
        ),
        .parameter_high = request->setup_packet->wIndex | (request->setup_packet->wLength << 16),
        .status = 0x8, // TRB Size = 0x8 (8 bytes, ALWAYS), Interrupter Target = 0x0
        .control = XHCI_TRB_TYPE_SETUP_STAGE
    };

    if (request->immediate_data_transfer) {
        setup_packet_trb.control |= XHCI_TRB_CONTROL_IDT;
    }

    switch (request->transfer_type) {
        case NO_DATA:
            setup_packet_trb.control |= XHCI_TRB_CONTROL_TRT_NO_DATA;
            break;
        case IN_DATA:
            setup_packet_trb.control |= XHCI_TRB_CONTROL_TRT_DATA_IN;
            break;
        case OUT_DATA:
            setup_packet_trb.control |= XHCI_TRB_CONTROL_TRT_DATA_OUT;
            break;
        default:
            console_write("Invalid tranfer type");
            return -1;
    }

    xhci_ep0_transfer_ring_enqueue(device, &setup_packet_trb);

    if (request->transfer_type != NO_DATA) {
        if (xhci_alloc_page(&transfer_job.data_buffer) < 0) {
            // TODO: Error here leaves the transfer ring in an invalid state, need to clean up.
            console_write("Failed to allocate data buffer for control transfer.");
            return -1;
        }

        xhci_trb_t data_stage_trb = {
            .parameter_low = (uint32_t)transfer_job.data_buffer.physical_address,
            .parameter_high = (uint32_t)(transfer_job.data_buffer.physical_address >> 32),
            .status = request->setup_packet->wLength, // Transfer Size = wLength
            .control = XHCI_TRB_DATA_STAGE_IN_DIRECTION | XHCI_TRB_TYPE_DATA_STAGE,
        };
        xhci_ep0_transfer_ring_enqueue(device, &data_stage_trb);
        transfer_job.has_data = 1;
    }

    xhci_trb_t status_stage_trb = {
        .parameter_low = 0,
        .parameter_high = 0,
        .status = 0x0,
        .control = XHCI_TRB_CONTROL_IOC | XHCI_TRB_TYPE_STATUS_STAGE, 
    };
    xhci_ep0_transfer_ring_enqueue(device, &status_stage_trb);

    transfer_job.callback = request->callback;
    transfer_job.callback_context = request->callback_context;
    transfer_job.transfer_length = request->setup_packet->wLength;

    // Enqueue the transfer job so the poller can process it.
    ring_buffer_t* transfer_jobs = &g_xhci_driver.devices[device->slot_id].endpoints[0].transfer_jobs;
    ring_buffer_enqueue(transfer_jobs, &transfer_job, sizeof(xhci_transfer_job_t));

    // Ring the doorbell to start the transfer.
    xhci_ep0_ring_doorbell(device);

    return 0;
}

int xhci_transfer(xhci_device_t *device, xhci_transfer_request_t *request) {
    xhci_transfer_job_t transfer_job;

    if (xhci_alloc_page(&transfer_job.data_buffer) < 0) {
        console_write("Failed to allocate data buffer for transfer.");
        return -1;
    }

    xhci_trb_t transfer_trb = {
        .parameter_low = (uint32_t)transfer_job.data_buffer.physical_address,
        .parameter_high = (uint32_t)(transfer_job.data_buffer.physical_address >> 32),
        .status = request->transfer_length, // Transfer Size = transfer_length
        .control = XHCI_TRB_TYPE_NORMAL | XHCI_TRB_CONTROL_IOC
    };

    if (request->interrupt_on_short_packet) {
        transfer_trb.control |= XHCI_TRB_CONTROL_ISP;
    }

    xhci_transfer_ring_enqueue(&request->endpoint->transfer_ring, &transfer_trb);

    xhci_device_state_t *device_state = &g_xhci_driver.devices[device->slot_id];
    xhci_endpoint_state_t *endpoint_state = &device_state->endpoints[request->endpoint->endpoint_context_idx - 1];

    transfer_job.callback = request->callback;
    transfer_job.callback_context = request->callback_context;
    transfer_job.transfer_length = request->transfer_length;
    transfer_job.has_data = 1;
    ring_buffer_enqueue(&endpoint_state->transfer_jobs, &transfer_job, sizeof(xhci_transfer_job_t));

    xhci_endpoint_ring_doorbell(device, request->endpoint);

    return 0;
}
