#pragma once

#include <stdint.h>

#include "../utils/linked_list.h"
#include "address_space.h"

// Forward declarations.
typedef struct thread_t thread_t;

typedef struct process_t {
    thread_t *main_thread;
    linked_list_t threads;
    address_space_t address_space;
    linked_list_t memory_pages;
} process_t;

int process_init(process_t *process);
int process_load_elf(process_t *process, const uint8_t *elf_buffer, uint64_t elf_size);
int process_start(process_t *process);
int process_destroy(process_t *process);
