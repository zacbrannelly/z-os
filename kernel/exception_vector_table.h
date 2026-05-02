#pragma once

#include <stdint.h>

typedef struct exception_frame_t {
    uint64_t registers[32];
} exception_frame_t;

// Forward declaration from `kernel/exception_vector_table.S`.
void exception_vector_table(void);

// Called when a sync exception occurs at the current EL with SP_EL1 active (e.g. syscall called from kernel mode).
void handle_current_el_sp1_sync_exception(exception_frame_t *frame, uint64_t esr, uint64_t elr, uint64_t spsr);

// Injects the exception vector table into the CPU.
int exception_vector_table_init(void);
