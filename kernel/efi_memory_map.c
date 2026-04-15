#include "efi_memory_map.h"

#include "console.h"

int efi_memory_map_is_usable(efi_memory_descriptor_t *descriptor) {
    return (
        descriptor->type == EFI_MEMORY_TYPE_CONVENTIONAL ||
        descriptor->type == EFI_MEMORY_TYPE_LOADER_CODE ||
        descriptor->type == EFI_MEMORY_TYPE_BOOT_SERVICE_CODE ||
        descriptor->type == EFI_MEMORY_TYPE_BOOT_SERVICE_DATA ||
        descriptor->type == EFI_MEMORY_TYPE_PERSISTENT_MEMORY
    );
}

void efi_memory_map_print_details(
    efi_memory_descriptor_t *efi_memory_map,
    uint64_t efi_memory_map_size,
    uint64_t efi_memory_map_descriptor_size
) {
    console_write("EFI Memory Map Ranges:\r\n");

    for (uint64_t i = 0; i < efi_memory_map_size; i += efi_memory_map_descriptor_size) {
        efi_memory_descriptor_t *descriptor =
            (efi_memory_descriptor_t *)((uint64_t)efi_memory_map + i);

        uint64_t phys_start = descriptor->physical_start_address;
        uint64_t phys_end   = phys_start + (descriptor->number_of_pages * 4096) - 1;
        uint64_t virt_start = descriptor->virtual_start_address;
        uint64_t virt_end   = descriptor->number_of_pages == 0
            ? virt_start
            : virt_start + (descriptor->number_of_pages * 4096) - 1;

        console_write("PA ");
        console_write_hex(phys_start);
        console_write(" - ");
        console_write_hex(phys_end);

        if (virt_start != 0 || descriptor->virtual_start_address != 0) {
            console_write("  VA ");
            console_write_hex(virt_start);
            console_write(" - ");
            console_write_hex(virt_end);
        }

        console_write("  ");

        switch (descriptor->type) {
            case 0:  console_write("Reserved"); break;
            case 1:  console_write("LoaderCode"); break;
            case 2:  console_write("LoaderData"); break;
            case 3:  console_write("BootServicesCode"); break;
            case 4:  console_write("BootServicesData"); break;
            case 5:  console_write("RuntimeServicesCode"); break;
            case 6:  console_write("RuntimeServicesData"); break;
            case 7:  console_write("ConventionalMemory"); break;
            case 8:  console_write("UnusableMemory"); break;
            case 9:  console_write("ACPIReclaimMemory"); break;
            case 10: console_write("ACPIMemoryNVS"); break;
            case 11: console_write("MemoryMappedIO"); break;
            case 12: console_write("MemoryMappedIOPortSpace"); break;
            case 13: console_write("PalCode"); break;
            case 14: console_write("PersistentMemory"); break;
            default:
                console_write("Type=");
                console_write_hex(descriptor->type);
                break;
        }

        console_write("  Pages ");
        console_write_hex(descriptor->number_of_pages);

        console_write("  Attr ");
        console_write_hex(descriptor->attribute);

        console_write("\r\n");
    }
}