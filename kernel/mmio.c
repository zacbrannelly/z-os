#include "mmio.h"
#include "mmap.h"

#define MMIO_BASE_ADDRESS 0xFFFF200000000000ULL
#define MMIO_PAGE_FLAGS PAGE_FLAG_EL1_RW | PAGE_FLAG_NX | PAGE_FLAG_DEVICE_MEMORY | PAGE_FLAG_NON_SHARABLE | PAGE_FLAG_ACCESS

int mmio_map_page(
    uint64_t physical_address,
    uint64_t* virtual_address
) {
    *virtual_address = MMIO_BASE_ADDRESS + physical_address;
    return mmap_map_page(
        physical_address,
        *virtual_address,
        MMIO_PAGE_FLAGS
    );
}

int mmio_map_region(
    uint64_t physical_start_address,
    uint64_t size,
    uint64_t* virtual_base_address
) {
    *virtual_base_address = MMIO_BASE_ADDRESS + physical_start_address;
    return mmap_map_range(
        *virtual_base_address,
        *virtual_base_address + size,
        physical_start_address,
        MMIO_PAGE_FLAGS
    );
}
