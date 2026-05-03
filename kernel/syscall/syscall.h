#pragma once

#include <stdint.h>
#include "../exception_vector_table.h"

#define SYSCALL_CONSOLE_WRITE 0x1
#define SYSCALL_YIELD 0x2
#define SYSCALL_TEST 123

// Calls a syscall by index.
uint64_t syscall_call(uint64_t syscall_idx, uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

// Handles a syscall.
void syscall_handler(exception_frame_t *frame);
