#pragma once

#include <stdint.h>

// Program header type for loadable segments.
#define PT_LOAD 0x1

// Program header flags.
#define PF_X (1 << 0) // Segment is executable
#define PF_W (1 << 1) // Segment is writable
#define PF_R (1 << 2) // Segment is readable

/**
* ELF64 header structure.
*/
typedef struct elf_header_t {
    // ELF identification bytes.
    uint8_t e_ident[16];
    // ELF file type (should be 0x2 - EXECUTABLE)
    uint16_t e_type;
    // Machine type.
    uint16_t e_machine;
    // ELF version.
    uint32_t e_version;
    uint64_t e_entry;
    // Program header table offset.
    uint64_t e_phoff;
    // Section header table offset.
    uint64_t e_shoff;
    uint32_t e_flags;
    // ELF header size
    uint16_t e_ehsize;
    // Program header table entry size.
    uint16_t e_phentsize;
    // Program header table entry count.
    uint16_t e_phnum;
    // Section header table entry size.
    uint16_t e_shentsize;
    // Section header table entry count.
    uint16_t e_shnum;
    // Section header table index of the section name string table.
    uint16_t e_shstrndx;
} elf_header_t;

/**
* ELF64 section header structure.
*/
typedef struct section_header_t {
    uint32_t sh_name_idx;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
} section_header_t;

/**
* ELF64 program header structure.
*/
typedef struct program_header_t {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} program_header_t;
