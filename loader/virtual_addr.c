#include "virtual_addr.h"

#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>

#define TCR_4KB_GRANULE_FLAG (1ULL << 31)
#define TCR_CLEAR_TG1_FLAG ~(0x3ULL << 30)

#define TCR_ENABLE_TTBR1_FLAG ~(1ULL << 23)
#define TCR_SET_T1SZ_16_FLAG (16ULL << 16)
#define TCR_CLEAR_T1SZ_FLAG ~(0x3FULL << 16)

int virtual_addr_allocate_table(EFI_SYSTEM_TABLE *SystemTable, virtual_addr_table_t *table) {
    // Clear the structure.
    ZeroMem((VOID *)table, sizeof(virtual_addr_table_t));

    // Allocate pages of memory for 4 levels of page tables.
    EFI_STATUS status = SystemTable->BootServices->AllocatePages(
      AllocateAnyPages,
      EfiLoaderData,
      1,
      (EFI_PHYSICAL_ADDRESS*)&table->root_page_table
    );
  
    if (EFI_ERROR(status)) {
      Print(L"Failed to allocate pages for root page table: %r\r\n", status);
      return -1;
    }

    ZeroMem((VOID *)(UINTN)table->root_page_table, VIRTUAL_ADDR_GRANULE_SIZE);

    return 0;
}

int alloc_new_table_entry(EFI_SYSTEM_TABLE *SystemTable, uint64_t *page_base_address, uint16_t index) {
    EFI_PHYSICAL_ADDRESS page_table = 0;
    EFI_STATUS status = SystemTable->BootServices->AllocatePages(
        AllocateAnyPages,
        EfiLoaderData,
        1,
        &page_table
    );

    if (EFI_ERROR(status)) {
        Print(L"Failed to allocate pages for page table at index %u: %r\r\n", index, status);
        return -1;
    }

    ZeroMem((VOID *)(UINTN)page_table, VIRTUAL_ADDR_GRANULE_SIZE);

    page_base_address[index] = (
        TBL_ENTRY_VALID |
        TBL_ENTRY_TABLE |
        (page_table & TBL_ENTRY_ADDR_MASK)
    );

    return 0;
}

int virtual_addr_map_page(
    EFI_SYSTEM_TABLE *SystemTable,
    virtual_addr_table_t *table,
    uint64_t physical_address,
    uint64_t virtual_address,
    uint64_t page_flags
) {
    // Extract level indices from the virtual address (48-bit address space).
    // 47..39 = L0 index
    // 38..30 = L1 index
    // 29..21 = L2 index
    // 20..12 = L3 index
    // 11..0 = Offset
    uint64_t l0_index = (virtual_address >> 39) & 0x1FF;
    uint64_t l1_index = (virtual_address >> 30) & 0x1FF;
    uint64_t l2_index = (virtual_address >> 21) & 0x1FF;
    uint64_t l3_index = (virtual_address >> 12) & 0x1FF;

    // Get the page table entry for the L0 index.
    uint64_t l0_entry = ((uint64_t *)table->root_page_table)[l0_index];
    if ((l0_entry & TBL_ENTRY_VALID) == 0) {
        // No link to the l1 page table, so we need to allocate a new one.
        if (alloc_new_table_entry(SystemTable, (uint64_t *)table->root_page_table, l0_index) < 0) {
            Print(L"Failed to allocate new table entry for L0 index %u: %r\r\n", l0_index);
            return -1;
        }
        l0_entry = ((uint64_t *)table->root_page_table)[l0_index];
    }

    uint64_t *l0_page_table = (uint64_t *)(l0_entry & TBL_ENTRY_ADDR_MASK);
    uint64_t l1_entry = l0_page_table[l1_index];
    if ((l1_entry & TBL_ENTRY_VALID) == 0) {
        if (alloc_new_table_entry(SystemTable, l0_page_table, l1_index) < 0) {
            Print(L"Failed to allocate new table entry for L1 index %u: %r\r\n", l1_index);
            return -1;
        }
        l1_entry = l0_page_table[l1_index];
    }

    uint64_t *l1_page_table = (uint64_t *)(l1_entry & TBL_ENTRY_ADDR_MASK);
    uint64_t l2_entry = l1_page_table[l2_index];
    if ((l2_entry & TBL_ENTRY_VALID) == 0) {
        if (alloc_new_table_entry(SystemTable, l1_page_table, l2_index) < 0) {
            Print(L"Failed to allocate new table entry for L2 index %u: %r\r\n", l2_index);
            return -1;
        }
        l2_entry = l1_page_table[l2_index];
    }

    uint64_t *l2_page_table = (uint64_t *)(l2_entry & TBL_ENTRY_ADDR_MASK);
    l2_page_table[l3_index] = (
        TBL_ENTRY_VALID |
        TBL_ENTRY_TABLE |
        PAGE_FLAG_ACCESS |
        page_flags |
        (physical_address & TBL_ENTRY_ADDR_MASK)
    );

    return 0;
}

int virtual_addr_map(
    EFI_SYSTEM_TABLE *SystemTable,
    virtual_addr_table_t *table,
    uint64_t physical_address,
    uint64_t virtual_address,
    uint64_t num_pages,
    uint64_t page_flags
) {
    // TODO: Validate that virtual address is aligned to 4KB.
    for (int i = 0; i < num_pages; i++) {
        int status = virtual_addr_map_page(
            SystemTable,
            table,
            physical_address + i * VIRTUAL_ADDR_GRANULE_SIZE,
            virtual_address + i * VIRTUAL_ADDR_GRANULE_SIZE,
            page_flags
        );
        if (status < 0) {
            Print(L"Failed to map page %u: %r\r\n", i, status);
            return -1;
        }
    }

    return 0;
}

uint64_t read_tcr() {
    uint64_t tcr = 0;
    __asm__("mrs %0, tcr_el1" : "=r" (tcr));
    return tcr;
}

int virtual_addr_apply_to_ttbr1(virtual_addr_table_t *table) {
    uint64_t ttbr1 = table->root_page_table & TBL_ENTRY_ADDR_MASK;
    uint64_t tcr = read_tcr();

    // Set the TG1 bits to 0x2 to indicate 4kb granule size.
    tcr &= TCR_CLEAR_TG1_FLAG;
    tcr |= TCR_4KB_GRANULE_FLAG;

    // Clear the EPD1 bit to enable TTBR1 translations.
    tcr &= TCR_ENABLE_TTBR1_FLAG;

    // Set the T1SZ bits to 16 to indicate a 48 bit address space (64 - 16 = 48).
    tcr &= TCR_CLEAR_T1SZ_FLAG;
    tcr |= TCR_SET_T1SZ_16_FLAG;

    // Make sure all table writes hit memory before the CPU can walk them.
    __asm__ volatile("dsb ishst" ::: "memory");
    __asm__ volatile("isb");
  
    // Program the translation controls first.
    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr) : "memory");
    __asm__ volatile("isb");
  
    // Install the new TTBR1 root.
    __asm__ volatile("msr ttbr1_el1, %0" :: "r"(ttbr1) : "memory");
    __asm__ volatile("isb");
  
    // Invalidate stale EL1 stage-1 translations.
    __asm__ volatile("tlbi vmalle1");
    __asm__ volatile("dsb ish");
    __asm__ volatile("isb");

    return 0;
}
