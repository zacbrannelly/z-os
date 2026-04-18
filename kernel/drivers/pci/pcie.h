#pragma once

#include "../acpi/acpi.h"

#include <stdint.h>

#define PCI_MAX_BUSES 256
#define PCI_MAX_DEVICES 32
#define PCI_MAX_FUNCTIONS 8

#define PCI_CONFIG_ADDRESS 0x0c
#define PCI_CONFIG_DATA 0x08
#define PCI_CONFIG_BAR0 0x10
#define PCI_CONFIG_BAR1 0x14
#define PCI_CONFIG_BAR2 0x18
#define PCI_CONFIG_BAR3 0x1c
#define PCI_CONFIG_BAR4 0x20
#define PCI_CONFIG_BAR5 0x24
#define PCI_CONFIG_COMMAND 0x04

#define PCI_COMMAND_MEMORY_SPACE_ENABLE (1 << 1)
#define PCI_COMMAND_BUS_MASTER_ENABLE (1 << 2)

#define PCI_CLASS_SERIAL_BUS_CONTROLLER 0x0c
#define PCI_SUBCLASS_USB_CONTROLLER 0x03
#define PCI_PROG_INT_USB_3_0_XHCI 0x30

int pcie_init(acpi_table_mcfg_entry_t *mcfg_entry);

// TODO: Add segment to these functions.
uint8_t pcie_config_read8(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset);
uint16_t pcie_config_read16(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset);
uint32_t pcie_config_read32(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset);

void pcie_config_write8(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset, uint8_t value);
void pcie_config_write16(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset, uint16_t value);
void pcie_config_write32(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset, uint32_t value);

int pcie_locate_device(
    uint8_t header_type,
    uint8_t class_code,
    uint8_t subclass_code,
    uint8_t prog_int,
    uint32_t *bus,
    uint32_t *device,
    uint32_t *function
);
