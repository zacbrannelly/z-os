#include "pcie.h"

#include <stddef.h>

static acpi_table_mcfg_entry_t *g_mcfg_entry = NULL;

uint64_t pcie_ecam_address(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset) {
    // TODO: Do we need the start number subtracted from the bus number? Or is this a LLM hallucination?
    return g_mcfg_entry->base_address + ((bus - g_mcfg_entry->start_bus_number) << 20) + (device << 15) + (function << 12) + offset;
}

int pcie_init(acpi_table_mcfg_entry_t *mcfg_entry) {
    g_mcfg_entry = mcfg_entry;
    return 0;
}

uint8_t pcie_config_read8(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset) {
    uint64_t address = pcie_ecam_address(bus, device, function, offset);
    return *(volatile uint8_t *)address;
}

uint16_t pcie_config_read16(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset) {
    uint64_t address = pcie_ecam_address(bus, device, function, offset);
    return *(volatile uint16_t *)address;
}

uint32_t pcie_config_read32(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset) {
    uint64_t address = pcie_ecam_address(bus, device, function, offset);
    return *(volatile uint32_t *)address;
}

void pcie_config_write8(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset, uint8_t value) {
    uint64_t address = pcie_ecam_address(bus, device, function, offset);
    *(volatile uint8_t *)address = value;
}

void pcie_config_write16(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset, uint16_t value) {
    uint64_t address = pcie_ecam_address(bus, device, function, offset);
    *(volatile uint16_t *)address = value;
}

void pcie_config_write32(uint32_t bus, uint32_t device, uint32_t function, uint32_t offset, uint32_t value) {
    uint64_t address = pcie_ecam_address(bus, device, function, offset);
    *(volatile uint32_t *)address = value;
}

int pcie_locate_device(
    uint8_t header_type,
    uint8_t class_code,
    uint8_t subclass_code,
    uint8_t prog_int,
    uint32_t *bus,
    uint32_t *device,
    uint32_t *function
) {
    // TODO: This is not a complete enumeration of all devices, it only enumerates the devices on the current bus.
    // TODO: Support PCI bridges.
    for (uint32_t current_bus = 0; current_bus < PCI_MAX_BUSES; current_bus++) {
        for (uint32_t current_device = 0; current_device < PCI_MAX_DEVICES; current_device++) {
            for (uint32_t current_function = 0; current_function < PCI_MAX_FUNCTIONS; current_function++) {
                uint32_t header_type = pcie_config_read32(current_bus, current_device, current_function, PCI_CONFIG_ADDRESS);
                uint32_t value = pcie_config_read32(current_bus, current_device, current_function, PCI_CONFIG_DATA);

                uint8_t current_class_code = (value >> 24) & 0xff;
                uint8_t current_subclass_code = (value >> 16) & 0xff;
                uint8_t current_prog_int = (value >> 8) & 0xff;

                if (
                    header_type == 0x0 &&
                    class_code == current_class_code &&
                    subclass_code == current_subclass_code &&
                    prog_int == current_prog_int) {
                    *bus = current_bus;
                    *device = current_device;
                    *function = current_function;
                    return 0;
                }
            }
        }
    }

    return -1;
}
