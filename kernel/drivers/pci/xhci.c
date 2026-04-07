#include "xhci.h"
#include "pcie.h"
#include "../../console.h"
#include "../../format.h"

#include <stdint.h>
#include <stddef.h>

#define XHCI_OP_USBCMD 0x0
#define XHCI_OP_USBSTS 0x4
#define XHCI_OP_USBSTS_HCRST (1 << 1)

#define XHCI_CAP_HCSPARAMS1 0x4

static uint8_t *g_mmio_base = NULL;
static uint8_t *g_caps = NULL;
static uint8_t *g_ops = NULL;
static uint8_t g_cap_length = 0;
static uint32_t g_bus = 0;
static uint32_t g_device = 0;
static uint32_t g_function = 0;

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

    return 0;
}

uint32_t xhci_read_op_register(uint32_t offset) {
    return *(volatile uint32_t*)(g_ops + offset);
}

void xhci_write_op_register(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(g_ops + offset) = value;
}

uint32_t xhci_read_cap_register(uint32_t offset) {
    return *(volatile uint32_t*)(g_caps + offset);
}

void xhci_write_cap_register(uint32_t offset, uint32_t value) {
    *(volatile uint32_t*)(g_caps + offset) = value;
}

int xhci_is_halted(void) {
    uint32_t usb_sts = xhci_read_op_register(XHCI_OP_USBSTS);
    return (usb_sts & 0x1) != 0;
}

void xhci_wait_for_reset(void) {
    // Spin until the HCRST bit is cleared, this will clear when the controller is reset.
    while ((xhci_read_op_register(XHCI_OP_USBCMD) & XHCI_OP_USBSTS_HCRST) > 0) {}
}

int xhci_init(void) {
    if (xhci_locate_device() != 0) {
        return -1;
    }

    char buffer[17];

    uint32_t hcs_params_s1 = xhci_read_cap_register(XHCI_CAP_HCSPARAMS1);
    console_write("xHCI HCS params S1: 0x");
    format_hex(buffer, sizeof(buffer), hcs_params_s1);
    console_write(buffer);
    console_write("\r\n");

    uint8_t max_device_slots = hcs_params_s1 & 0xff;
    uint8_t max_ports = (hcs_params_s1 >> 24) & 0xff;

    console_write("xHCI max device slots: 0x");
    format_hex(buffer, sizeof(buffer), max_device_slots);
    console_write(buffer);
    console_write("\r\n");

    console_write("xHCI max ports: 0x");
    format_hex(buffer, sizeof(buffer), max_ports);
    console_write(buffer);
    console_write("\r\n");

    uint32_t usb_cmd = xhci_read_op_register(XHCI_OP_USBCMD);
    console_write("xHCI USB CMD: 0x");
    format_hex(buffer, sizeof(buffer), usb_cmd);
    console_write(buffer);
    console_write("\r\n");

    uint32_t usb_sts = xhci_read_op_register(XHCI_OP_USBSTS);
    console_write("xHCI USB STS: 0x");
    format_hex(buffer, sizeof(buffer), usb_sts);
    console_write(buffer);
    console_write("\r\n");

    // Reset the controller (set the USBCMD.HCRST bit)
    xhci_write_op_register(XHCI_OP_USBCMD, usb_cmd | XHCI_OP_USBSTS_HCRST);
    xhci_wait_for_reset();

    console_write("xHCI controller reset complete\r\n");

    return 0;
}
