#include "kernel_loader.h"
#include "kernel_elf.h"
#include "elf.h"

#include <Library/UefiLib.h>

int kernel_loader_load(EFI_SYSTEM_TABLE *system_table, virtual_addr_table_t *table, kernel_elf_info_t *elf_info) {
    for (int i = 0; i < elf_info->num_segments; i++) {
        kernel_elf_segment_t *segment = &elf_info->segments[i];

        // Allocate pages of memory for the kernel based on the info.
        uint64_t num_pages = segment->loadable_segment_memory_size / EFI_PAGE_SIZE;
        if (segment->loadable_segment_memory_size % EFI_PAGE_SIZE != 0) {
            num_pages++;
        }

        EFI_PHYSICAL_ADDRESS kernel_pages = 0;
        EFI_STATUS status = system_table->BootServices->AllocatePages(
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
        system_table->BootServices->CopyMem(
            (VOID *)kernel_pages,
            (VOID *)segment->loadable_segment_start,
            segment->loadable_segment_file_size
        );

        // Zero out the BSS section of the kernel (memory_size - file_size).
        system_table->BootServices->SetMem(
            (VOID *)(kernel_pages + segment->loadable_segment_file_size),
            segment->loadable_segment_memory_size - segment->loadable_segment_file_size,
            0
        );

        // Determine the page flags for the kernel image.
        uint64_t page_flags = PAGE_FLAG_INNER_SHARABLE | PAGE_FLAG_MAIR_ATTR(3ULL);
        if ((segment->flags & PF_X) == 0) {
            // Not executable, so set the PXN and UXN flags.
            page_flags |= PAGE_FLAG_PXN | PAGE_FLAG_UXN;
        }
        if ((segment->flags & PF_W) == 0) {
            // Not writable, so set the EL1_RO flag.
            page_flags |= PAGE_FLAG_EL1_RO;
        } else {
            // Writable, so set the EL1_RW flag.
            page_flags |= PAGE_FLAG_EL1_RW;
        }

        // Map the kernel image into the virtual address space.
        virtual_addr_map(
            system_table,
            table,
            kernel_pages,
            segment->image_start,
            num_pages,
            page_flags
        );
    }

    return 0;
}
