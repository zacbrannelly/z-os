#include "acpi.h"
#include "../../assert.h"
#include "../../mmap.h"
#include "../../string.h"
#include "../../console.h"

#include <stddef.h>

#define ACPI_VIRTUAL_BASE_ADDRESS 0xFFFF300000000000ULL
#define ACPI_PAGE_FLAGS PAGE_FLAG_EL1_RW | PAGE_FLAG_NX | PAGE_FLAG_NORMAL_MEMORY | PAGE_FLAG_INNER_SHARABLE | PAGE_FLAG_ACCESS

static acpi_rsdp_t *g_rsdp = NULL;
static acpi_table_mcfg_t *g_mcfg = NULL;

static void acpi_map_page(uint64_t physical_address, uint64_t *virtual_address) {
    *virtual_address = ACPI_VIRTUAL_BASE_ADDRESS + physical_address;
    assert(mmap_map_range(*virtual_address, *virtual_address + MMAP_GRANULE_SIZE, physical_address, ACPI_PAGE_FLAGS) == 0);
}

int acpi_init(void *acpi_table) {
    acpi_map_page((uint64_t)acpi_table, (uint64_t *)&g_rsdp);
    return 0;
}

acpi_rsdp_t *acpi_get_rsdp(void) {
    return g_rsdp;
}

acpi_xsdt_t *acpi_get_xsdt(void) {
    // TODO: Map according to the length of the table.
    uint64_t xsdt_physical_address = g_rsdp->xsdt_address;
    uint64_t xsdt_virtual_address = 0;
    acpi_map_page(xsdt_physical_address, &xsdt_virtual_address);
    return (acpi_xsdt_t *)xsdt_virtual_address;
}

acpi_table_header_t *acpi_get_table_by_signature(const char *signature) {
    acpi_xsdt_t *xsdt = acpi_get_xsdt();
    uint64_t *xsdt_entries = (uint64_t *)((uint64_t)xsdt + sizeof(acpi_xsdt_t));

    for (int i = 0; i < (xsdt->length - sizeof(acpi_xsdt_t)) / sizeof(uint64_t); i++) {
        uint64_t table_physical_address = xsdt_entries[i];
        uint64_t table_virtual_address = ACPI_VIRTUAL_BASE_ADDRESS + table_physical_address;
        acpi_map_page(table_physical_address, &table_virtual_address);

        acpi_table_header_t *table = (acpi_table_header_t *)(table_virtual_address);
        if (strncmp(table->signature, signature, 4) == 0) {
            return table;
        }
    }

    return NULL;
}

acpi_table_mcfg_t *acpi_get_mcfg(void) {
    if (g_mcfg != NULL) {
        return g_mcfg;
    }

    acpi_table_header_t *table = acpi_get_table_by_signature("MCFG");
    if (table == NULL) {
        return NULL;
    }

    g_mcfg = (acpi_table_mcfg_t *)table;
    return g_mcfg;
}

acpi_table_mcfg_entry_t *acpi_get_mcfg_entry(void) {
    acpi_table_mcfg_t *mcfg = acpi_get_mcfg();
    if (mcfg == NULL) {
        return NULL;
    }

    if (mcfg->length == sizeof(acpi_table_mcfg_t)) {
        return NULL;
    }

    return (acpi_table_mcfg_entry_t *)((uint64_t)mcfg + sizeof(acpi_table_mcfg_t));
}

acpi_table_spcr_t *acpi_get_spcr(void) {
    acpi_table_header_t *table = acpi_get_table_by_signature("SPCR");
    if (table == NULL) {
        return NULL;
    }

    return (acpi_table_spcr_t *)table;
}
