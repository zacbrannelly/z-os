#pragma once

#include <stdint.h>

// Pack the structs to avoid alignment padding being inserted.
#pragma pack(push, 1)

typedef struct acpi_rsdp_t {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    char reserved[3];
} acpi_rsdp_t;

typedef struct acpi_xsdt_t {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_xsdt_t;

typedef struct acpi_table_header_t {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_table_header_t;

typedef struct acpi_table_mcfg_t {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
    uint64_t reserved;
} acpi_table_mcfg_t;

typedef struct acpi_table_mcfg_entry_t {
    uint64_t base_address;
    uint16_t segment_group;
    uint8_t start_bus_number;
    uint8_t end_bus_number;
    uint32_t reserved;
} acpi_table_mcfg_entry_t;

typedef struct acpi_generic_address_t {
    uint8_t address_space_id;
    uint8_t register_bit_width;
    uint8_t register_bit_offset;
    uint8_t access_size;
    uint64_t address;
} acpi_generic_address_t;

typedef struct acpi_table_spcr_t {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
    uint8_t interface_type;
    uint8_t reserved0[3];
    acpi_generic_address_t base_address;
    // There are other fields in the table, but we don't need them for now.
} acpi_table_spcr_t;

#pragma pack(pop)

int acpi_init(void *acpi_table);

acpi_rsdp_t *acpi_get_rsdp(void);
acpi_xsdt_t *acpi_get_xsdt(void);
acpi_table_header_t *acpi_get_table_by_signature(const char *signature);
acpi_table_mcfg_t *acpi_get_mcfg(void);
acpi_table_mcfg_entry_t *acpi_get_mcfg_entry(void);
acpi_table_spcr_t *acpi_get_spcr(void);
