#pragma once

#include <Uefi.h>
#include "kernel_elf.h"
#include "virtual_addr.h"

int kernel_loader_load(
    EFI_SYSTEM_TABLE *SystemTable,
    virtual_addr_table_t *table,
    kernel_elf_info_t *elf_info
);
