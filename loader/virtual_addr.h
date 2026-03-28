#pragma once

#include <Uefi.h>
#include <stdint.h>

#define VIRTUAL_ADDR_GRANULE_SIZE 4096

#define TBL_ENTRY_VALID 0x1ULL
#define TBL_ENTRY_TABLE 0x2ULL
#define TBL_ENTRY_ADDR_MASK 0x0000FFFFFFFFF000ULL

#define PAGE_FLAG_ACCESS (1ULL << 10)
#define PAGE_FLAG_INNER_SHARABLE (3ULL << 8)
#define PAGE_FLAG_UXN (1ULL << 54) // UXN = Unprivileged Execute Never
#define PAGE_FLAG_PXN (1ULL << 53) // PXN = Privileged Execute Never
#define PAGE_FLAG_EL0_EL1_RO (0x3ULL << 6) // Read Only (for EL0 and EL1)
#define PAGE_FLAG_EL1_RO (0x2ULL << 6) // Privilged Read Only (for EL1)
#define PAGE_FLAG_EL0_EL1_RW (0x1ULL << 6) // Read/Write (for EL0 and EL1)
#define PAGE_FLAG_EL1_RW (0x0ULL << 6) // Privilged Read/Write (for EL1)
#define PAGE_FLAG_MAIR_ATTR(attr) (attr << 2)

/**
UEFI setup of MAIR_EL1 register:
AttrIndx 0 = 0x00: Device-nGnRnE (no gathering, no reordering, no execution)
AttrIndx 1 = 0x44: Normal Non-cacheable
AttrIndx 2 = 0xBB: Normal Write-Through, Read/Write-Allocate
AttrIndx 3 = 0xFF: Normal Write-Back, Read/Write-Allocate
*/
// TODO: Prepare my own MAIR_EL1 register.
#define DEFAULT_PAGE_FLAGS PAGE_FLAG_INNER_SHARABLE | PAGE_FLAG_EL1_RO | PAGE_FLAG_MAIR_ATTR(3ULL)

typedef struct virtual_addr_table_t {
    uint64_t root_page_table;
} virtual_addr_table_t;

int virtual_addr_allocate_table(EFI_SYSTEM_TABLE *system_table, virtual_addr_table_t *table);

int virtual_addr_map(
    EFI_SYSTEM_TABLE *system_table,
    virtual_addr_table_t *table,
    uint64_t physical_address,
    uint64_t virtual_address,
    uint64_t num_pages,
    uint64_t page_flags
);

int virtual_addr_apply_to_ttbr1(virtual_addr_table_t *table);
