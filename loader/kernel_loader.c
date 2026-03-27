#include "kernel_loader.h"
#include "kernel_elf.h"

#include <Library/UefiLib.h>

static void sync_kernel_code_for_exec(UINTN start, UINTN size) {
    UINT64 ctr = 0;
    UINTN dline = 0;
    UINTN addr = 0;
    UINTN end = start + size;

    __asm__ volatile("mrs %0, ctr_el0" : "=r"(ctr));
    dline = 4u << ((ctr >> 16) & 0xF);

    for (addr = start & ~(dline - 1); addr < end; addr += dline) {
        __asm__ volatile("dc cvau, %0" :: "r"(addr) : "memory");
    }

    __asm__ volatile("dsb ish" ::: "memory");
    __asm__ volatile("ic iallu" ::: "memory");
    __asm__ volatile("dsb ish" ::: "memory");
    __asm__ volatile("isb");
}


int kernel_loader_load(EFI_SYSTEM_TABLE *SystemTable, virtual_addr_table_t *table, kernel_elf_info_t *elf_info) {
    // Allocate pages of memory for the kernel based on the info.
    uint64_t num_pages = elf_info->loadable_segment_memory_size / EFI_PAGE_SIZE;
    if (elf_info->loadable_segment_memory_size % EFI_PAGE_SIZE != 0) {
        num_pages++;
    }

    EFI_PHYSICAL_ADDRESS kernel_pages = 0;
    EFI_STATUS status = SystemTable->BootServices->AllocatePages(
        AllocateAnyPages,
        EfiLoaderData,
        num_pages,
        &kernel_pages
    );

    if (EFI_ERROR(status)) {
        Print(L"Failed to allocate pages for kernel: %r\r\n", status);
        return -1;
    }

    // Copy the kernel image into the allocated memory.
    SystemTable->BootServices->CopyMem(
        (VOID *)kernel_pages,
        (VOID *)elf_info->loadable_segment_start,
        elf_info->loadable_segment_file_size
    );

    // Zero out the BSS section of the kernel (memory_size - file_size).
    SystemTable->BootServices->SetMem(
        (VOID *)(kernel_pages + elf_info->loadable_segment_file_size),
        elf_info->loadable_segment_memory_size - elf_info->loadable_segment_file_size,
        0
    );

    sync_kernel_code_for_exec((UINTN)kernel_pages, (UINTN)elf_info->loadable_segment_memory_size);

    // TODO: Map the kernel image into the virtual address space.
    virtual_addr_map(SystemTable, table, kernel_pages, elf_info->image_start, num_pages, DEFAULT_PAGE_FLAGS);

    return 0;
}
