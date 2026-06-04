#pragma once

#include <stdint.h>
#include <libz/handle.h>

#include "../utils/linked_list.h"
#include "address_space.h"
#include "fd_table.h"

// Forward declarations.
typedef struct thread_t thread_t;

typedef enum process_memory_page_type_t {
    PROCESS_MEMORY_PAGE_TYPE_USER_STACK,
    PROCESS_MEMORY_PAGE_TYPE_PROGRAM
} process_memory_page_type_t;

typedef struct process_memory_page_t {
    uint64_t virtual_address;
    uint64_t physical_address;
    process_memory_page_type_t type;
} process_memory_page_t;

typedef struct mmap_entry_t {
    uint64_t virtual_address;
    uint64_t num_pages;
} mmap_entry_t;

typedef struct process_t {
    handle_t handle;
    thread_t *main_thread;
    linked_list_t threads;
    address_space_t address_space;
    linked_list_t memory_pages;
    linked_list_t mmap_entries;
    fd_table_t fd_table;
} process_t;

int process_init(process_t *process);
int process_load_elf(process_t *process, const uint8_t *elf_buffer, uint64_t elf_size);
int process_start(process_t *process);
int process_destroy(process_t *process);
