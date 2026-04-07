#pragma once

#include <stdint.h>

typedef struct boot_info {
    // Framebuffer information.
    uint32_t *framebuffer;
    uint32_t framebuffer_size;
    uint32_t framebuffer_width;
    uint32_t framebuffer_stride;

    // Memory map provided by the bootloader.
    void* memory_map;
    uint64_t memory_map_size;
    uint64_t memory_map_descriptor_size;

    // ACPI table provided by the bootloader.
    void* acpi_table;
} boot_info_t;
