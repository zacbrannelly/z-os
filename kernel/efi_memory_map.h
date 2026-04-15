#pragma once

#include <stdint.h>

#define EFI_PAGE_SIZE 4096

// LOADER_DATA is technically usable for page allocation, but it is used during the loader stage, so avoid using it for now.
#define EFI_MEMORY_TYPE_LOADER_DATA 0x2

// EFI memory types that are usable for page allocation.
#define EFI_MEMORY_TYPE_LOADER_CODE 0x1
#define EFI_MEMORY_TYPE_BOOT_SERVICE_CODE 0x3
#define EFI_MEMORY_TYPE_BOOT_SERVICE_DATA 0x4
#define EFI_MEMORY_TYPE_PERSISTENT_MEMORY 0xe
#define EFI_MEMORY_TYPE_CONVENTIONAL 0x7

typedef struct efi_memory_descriptor_t {
    uint32_t type;
    uint64_t physical_start_address;
    uint64_t virtual_start_address;
    uint64_t number_of_pages;
    uint64_t attribute;
} efi_memory_descriptor_t;

void efi_memory_map_print_details(
    efi_memory_descriptor_t *efi_memory_map,
    uint64_t efi_memory_map_size,
    uint64_t efi_memory_map_descriptor_size
);

int efi_memory_map_is_usable(efi_memory_descriptor_t *descriptor);
