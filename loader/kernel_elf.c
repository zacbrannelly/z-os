#include "kernel_elf.h"
#include "elf.h"

#include <Library/UefiLib.h>

#define VERBOSE 0

int kernel_elf_parse_loadable_segments(uint8_t *elf_buffer, uint64_t elf_buffer_size, kernel_elf_info_t *info) {
    elf_header_t *elf_header = (elf_header_t *)elf_buffer;

    if (elf_buffer_size < elf_header->e_phoff + elf_header->e_phnum * elf_header->e_phentsize) {
        Print(L"ELF file is too small to contain the program header table: %lu < %lu\r\n", elf_buffer_size, elf_header->e_phoff + elf_header->e_phnum * elf_header->e_phentsize);
        return -1;
    }

    // Fetch the entry point from the ELF header.
    info->entry_point = elf_header->e_entry;

    // Fetch each loadable segment from the ELF header.
    uint8_t loadable_segment_index = 0;
    for (int i = 0; i < elf_header->e_phnum; i++) {
        program_header_t *program = (program_header_t *)(elf_buffer + elf_header->e_phoff + i * elf_header->e_phentsize);
        if (program->p_type == PT_LOAD) {
            if (loadable_segment_index >= KERNEL_ELF_MAX_SEGMENTS) {
                Print(L"Too many loadable segments in ELF file: %d\r\n", loadable_segment_index);
                return -1;
            }

            kernel_elf_segment_t segment;
            segment.loadable_segment_start = elf_buffer + program->p_offset;
            segment.loadable_segment_memory_size = program->p_memsz;
            segment.loadable_segment_file_size = program->p_filesz;
            segment.image_start = program->p_vaddr;
            segment.alignment = program->p_align;
            segment.flags = program->p_flags;

            if (program->p_filesz > program->p_memsz) {
                Print(L"ELF file has a loadable segment with a file size greater than the memory size: %d\r\n", program->p_filesz);
                return -1;
            }

            if (program->p_align == 0) {
                Print(L"ELF file has a loadable segment with an alignment of 0: %d\r\n", program->p_align);
                return -1;
            }

            info->segments[loadable_segment_index++] = segment;
        }
    }

    if (loadable_segment_index == 0) {
        return -1;
    }

    info->num_segments = loadable_segment_index;
    return 0;
}

int kernel_elf_parse_info(uint8_t *elf_buffer, uint64_t elf_buffer_size, kernel_elf_info_t *info) {
    // TODO: Check our assumptions about the ELF file are correct (64-bit, little endian byte order).
    if (VERBOSE) {
        elf_print_details(elf_buffer);
    }

    info->entry_point = 0;
    info->num_segments = 0;
    return kernel_elf_parse_loadable_segments(elf_buffer, elf_buffer_size, info);
}
