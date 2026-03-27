#pragma once

#include <stdint.h>

typedef struct kernel_elf_info_t {
    // Where to jump to once the kernel is loaded into memory (in virtual address space).
    uint64_t entry_point;

    // Where the kernel segment starts in the provided ELF buffer.
    uint8_t *loadable_segment_start;

    // Size of the kernel segment in the provided ELF buffer.
    uint64_t loadable_segment_memory_size;

    // Size of data to copy from the ELF buffer.
    uint64_t loadable_segment_file_size;

    // Where to copy the kernel segment to in memory (in virtual address space).
    uint64_t image_start;

    // Alignment requirement for the program image.
    uint64_t program_alignment;
} kernel_elf_info_t;

int kernel_elf_parse_info(uint8_t *elf_buffer, kernel_elf_info_t *info);
