#include "process.h"
#include "elf.h"
#include "../assert.h"
#include "../page_alloc.h"
#include "../kmalloc.h"
#include "../memory.h"
#include "../mmap.h"
#include "../scheduler/thread.h"
#include "../files/file.h"
#include "../files/file_table.h"

#include <stddef.h>

#define USER_STACK_TOP 0x0000007ffffff000ULL
#define USER_STACK_BASE (USER_STACK_TOP - USER_STACK_SIZE)
#define USER_STACK_SIZE (1ULL << 16) // 64KiB
#define USER_STACK_PAGE_FLAGS PAGE_FLAG_NX | PAGE_FLAG_EL0_EL1_RW | PAGE_FLAG_NORMAL_MEMORY | PAGE_FLAG_INNER_SHARABLE

static int allocate_memory_pages(
    process_t *process,
    process_memory_page_type_t type,
    uint64_t virtual_base,
    uint64_t size,
    uint64_t page_flags,
    uint8_t *buffer,
    uint64_t buffer_size
) {
    uint64_t num_pages = size / PAGE_SIZE;
    if (size % PAGE_SIZE != 0) {
        num_pages++;
    }

    uint64_t bytes_remaining = buffer_size;
    for (uint64_t i = 0; i < num_pages; i++) {
        process_memory_page_t *page = (process_memory_page_t *)kmalloc(sizeof(process_memory_page_t));
        if (page == NULL) {
            return -1;
        }
        page->virtual_address = virtual_base + i * PAGE_SIZE;
        page->physical_address = 0;
        page->type = type;

        uint64_t existing_physical_address = 0;
        if (vmap_virtual_to_physical(&process->address_space.page_table, page->virtual_address, &existing_physical_address) == 0) {
            // TODO: Resolve conflicting page flags for existing mapped pages.
            page->physical_address = existing_physical_address;
        } else {
            if (page_alloc_block(PAGE_ALLOC_ORDER_4KB, &page->physical_address) < 0) {
                kfree(page);
                return -1;
            }
    
            if (vmap_map_page(
                &process->address_space.page_table,
                page->physical_address,
                page->virtual_address,
                page_flags
            ) < 0) {
                page_alloc_free(page->physical_address, PAGE_ALLOC_ORDER_4KB);
                kfree(page);
                return -1;
            }
        }

        linked_list_node_t *node = NULL;
        linked_list_insert(&process->memory_pages, page, &node);

        uint64_t virtual_address = 0;
        if (mmap_physical_to_virtual(page->physical_address, &virtual_address) < 0) {
            return -1;
        }

        if (buffer != NULL && bytes_remaining > 0) {
            if (bytes_remaining < PAGE_SIZE) {
                memory_copy((void *)virtual_address, (void *)buffer, bytes_remaining);
                bytes_remaining = 0;
                buffer = NULL;
            } else {
                memory_copy((void *)virtual_address, (void *)buffer, PAGE_SIZE);
                bytes_remaining -= PAGE_SIZE;
                buffer += PAGE_SIZE;
            }
        } else {
            // TODO: Figure out how to handle overlapping program segments (they have the same / overlappping VA spaces).
            // TODO: memory_set((void *)virtual_address, 0, PAGE_SIZE);
        }
    }
    return 0;
}

int process_init(process_t *process) {
    memory_set(process, 0, sizeof(process_t));
    if (linked_list_init(&process->threads) < 0) {
        return -1;
    }
    if (linked_list_init(&process->memory_pages) < 0) {
        return -1;
    }

    if (address_space_init(&process->address_space) < 0) {
        return -1;
    }

    if (linked_list_init(&process->mmap_entries) < 0) {
        return -1;
    }

    if (fd_table_init(&process->fd_table) < 0) {
        return -1;
    }

    // Allocate & map the user stack.
    if (allocate_memory_pages(
        process,
        PROCESS_MEMORY_PAGE_TYPE_USER_STACK,
        USER_STACK_BASE,
        USER_STACK_SIZE,
        USER_STACK_PAGE_FLAGS,
        NULL,
        0
    ) < 0) {
        return -1;
    }

    // Register the process as a file in the global file table.
    file_t process_file;
    memory_set(&process_file, 0, sizeof(file_t));
    process_file.ref_count = 1;
    process_file.private_data = (void *)process;
    assert(file_table_open(NULL, process_file, &process->handle) == 0);

    return 0;
}

static int load_program_segment(process_t *process, const uint8_t *elf_buffer, uint64_t elf_size, program_header_t *program) {
    if (program->p_filesz > program->p_memsz) {
        return -1;
    }

    if (program->p_align == 0) {
        return -1;
    }
    
    uint64_t loadable_segment_start = (uint64_t)elf_buffer + program->p_offset;
    uint64_t loadable_segment_memory_size = program->p_memsz;
    uint64_t loadable_segment_file_size = program->p_filesz;
    uint64_t image_start = program->p_vaddr;
    uint64_t alignment = program->p_align;
    uint32_t flags = program->p_flags;

    uint32_t page_flags = PAGE_FLAG_INNER_SHARABLE | PAGE_FLAG_NORMAL_MEMORY;
    if ((flags & PF_X) == 0) {
        // Not executable, so set the PXN and UXN flags.
        page_flags |= PAGE_FLAG_PXN | PAGE_FLAG_UXN;
    }
    if ((flags & PF_W) == 0) {
        // Not writable, so set the EL1_RO flag.
        page_flags |= PAGE_FLAG_EL0_EL1_RO;
    } else {
        // Writable, so set the EL1_RW flag.
        page_flags |= PAGE_FLAG_EL0_EL1_RW;
    }

    if (allocate_memory_pages(
        process,
        PROCESS_MEMORY_PAGE_TYPE_PROGRAM,
        image_start,
        loadable_segment_memory_size,
        page_flags,
        (uint8_t *)loadable_segment_start,
        loadable_segment_file_size
    ) < 0) {
        return -1;
    }

    return 0;
}

int process_load_elf(process_t *process, const uint8_t *elf_buffer, uint64_t elf_size) {
    elf_header_t *elf_header = (elf_header_t *)elf_buffer;

    // TODO: Assert is executable.
    // TODO: Assert is 64-bit.
    // TODO: Assert is little endian.
    // TODO: Assert is fixed entry point (not PIE/PIC for now).

    uint64_t entry_point = elf_header->e_entry;

    for (int i = 0; i < elf_header->e_phnum; i++) {
        program_header_t *program = (program_header_t *)(elf_buffer + elf_header->e_phoff + i * elf_header->e_phentsize);
        if (program->p_type == PT_LOAD) {
            if (load_program_segment(process, elf_buffer, elf_size, program) < 0) {
                return -1;
            }
        }
    }

    thread_t *main_thread = (thread_t *)kmalloc(sizeof(thread_t));
    if (main_thread == NULL) {
        return -1;
    }
    if (thread_init(main_thread, entry_point, USER_STACK_TOP, THREAD_TYPE_USER) < 0) {
        return -1;
    }
    main_thread->process = process;

    linked_list_node_t *node = NULL;
    linked_list_insert(&process->threads, main_thread, &node);
    process->main_thread = main_thread;

    return 0;
}

int process_start(process_t *process) {
    assert(process->main_thread != NULL);
    return thread_start(process->main_thread);
}

int process_destroy(process_t *process) {
    // TODO: Implement.
    return 0;
}
