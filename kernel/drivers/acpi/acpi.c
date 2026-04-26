#include "acpi.h"
#include "../../string.h"
#include "../../console.h"

#include <stddef.h>

static acpi_rsdp_t *g_rsdp = NULL;
static acpi_table_mcfg_t *g_mcfg = NULL;

int acpi_init(void *acpi_table) {
    g_rsdp = (acpi_rsdp_t *)acpi_table;
    return 0;
}

acpi_rsdp_t *acpi_get_rsdp(void) {
    return g_rsdp;
}

acpi_xsdt_t *acpi_get_xsdt(void) {
    return (acpi_xsdt_t *)(g_rsdp->xsdt_address);
}

acpi_table_header_t *acpi_get_table_by_signature(const char *signature) {
    acpi_xsdt_t *xsdt = acpi_get_xsdt();
    uint64_t *xsdt_entries = (uint64_t *)((uint64_t)xsdt + sizeof(acpi_xsdt_t));

    for (int i = 0; i < (xsdt->length - sizeof(acpi_xsdt_t)) / sizeof(uint64_t); i++) {
        acpi_table_header_t *table = (acpi_table_header_t *)(xsdt_entries[i]);
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
