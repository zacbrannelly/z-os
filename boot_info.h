#pragma once

#include <stdint.h>

#define MAX_BOOT_MODULES 10

typedef struct boot_module_t {
    char name[64];
    uint8_t *elf_buffer;
    uint64_t elf_size;
} boot_module_t;

typedef struct boot_info_t {
    // Framebuffer information.
    uint32_t *framebuffer;
    uint32_t framebuffer_size;
    uint32_t framebuffer_width;
    uint32_t framebuffer_stride;

    // Memory map provided by the bootloader.
    void *memory_map;
    uint64_t memory_map_size;
    uint64_t memory_map_descriptor_size;

    // ACPI table provided by the bootloader.
    void *acpi_table;

    // Boot modules (programs to start on boot).
    boot_module_t boot_modules[MAX_BOOT_MODULES];
    uint64_t num_boot_modules;
} boot_info_t;
