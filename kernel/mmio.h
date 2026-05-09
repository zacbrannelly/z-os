#pragma once

#include <stdint.h>

int mmio_map_page(
    uint64_t physical_address,
    uint64_t* virtual_address
);

int mmio_map_region(
    uint64_t physical_start_address,
    uint64_t size,
    uint64_t* virtual_base_address
);
