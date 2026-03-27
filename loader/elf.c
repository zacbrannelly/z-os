#include "elf.h"

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>

void elf_print_details(uint8_t *elf_buffer) {
    elf_header_t *elf_header = (elf_header_t *)elf_buffer;
    Print(L"Type: 0x%x\r\n", elf_header->e_type);
    Print(L"Machine: 0x%x\r\n", elf_header->e_machine);
    Print(L"Version: 0x%x\r\n", elf_header->e_version);
    Print(L"Entry: 0x%lx\r\n", elf_header->e_entry);
    Print(L"Program header table offset: 0x%lx\r\n", elf_header->e_phoff);
    Print(L"Section header table offset: 0x%lx\r\n", elf_header->e_shoff);
    Print(L"Flags: 0x%x\r\n", elf_header->e_flags);
    Print(L"ELF header size: %lu\r\n", elf_header->e_ehsize);
    Print(L"Program header table entry size: %lu\r\n", elf_header->e_phentsize);
    Print(L"Program header table entry count: %lu\r\n", elf_header->e_phnum);
    Print(L"Section header table entry size: %lu\r\n", elf_header->e_shentsize);
    Print(L"Section header table entry count: %lu\r\n", elf_header->e_shnum);
    Print(L"Section header table index of the section name string table: 0x%x\r\n", elf_header->e_shstrndx);

    // section_header_t *section_header = (section_header_t *)(elf_buffer + elf_header->e_shoff);
    section_header_t *string_table_section_header = (section_header_t *)(elf_buffer + elf_header->e_shoff + elf_header->e_shstrndx * elf_header->e_shentsize);
    const char *string_table = (const char *)(elf_buffer + string_table_section_header->sh_offset);

    Print(L"String table: %s\r\n", string_table);

    for (int i = 0; i < elf_header->e_shnum; i++) {
        section_header_t *section = (section_header_t *)(elf_buffer + elf_header->e_shoff + i * elf_header->e_shentsize);
        Print(L"Section %d name: %a\r\n", i, &string_table[section->sh_name_idx]);

        Print(L"Section %d type: 0x%x\r\n", i, section->sh_type);
        Print(L"Section %d flags: 0x%lx\r\n", i, section->sh_flags);
        Print(L"Section %d address: 0x%lx\r\n", i, section->sh_addr);
        Print(L"Section %d offset: 0x%lx\r\n", i, section->sh_offset);
        Print(L"Section %d size: 0x%lx\r\n", i, section->sh_size);
        Print(L"Section %d link: 0x%x\r\n", i, section->sh_link);
        Print(L"Section %d info: 0x%x\r\n", i, section->sh_info);
        Print(L"Section %d addralign: 0x%lx\r\n", i, section->sh_addralign);
        Print(L"Section %d entsize: 0x%lx\r\n", i, section->sh_entsize);
    }

    for (int i = 0; i < elf_header->e_phnum; i++) {
        program_header_t *program = (program_header_t *)(elf_buffer + elf_header->e_phoff + i * elf_header->e_phentsize);
        Print(L"Program %d type: 0x%x\r\n", i, program->p_type);
        Print(L"Program %d flags: 0x%x\r\n", i, program->p_flags);
        Print(L"Program %d offset: 0x%lx\r\n", i, program->p_offset);
        Print(L"Program %d vaddr: 0x%lx\r\n", i, program->p_vaddr);
        Print(L"Program %d paddr: 0x%lx\r\n", i, program->p_paddr);
        Print(L"Program %d filesz: 0x%lx\r\n", i, program->p_filesz);
        Print(L"Program %d memsz: 0x%lx\r\n", i, program->p_memsz);
        Print(L"Program %d align: 0x%lx\r\n", i, program->p_align);
    }
}