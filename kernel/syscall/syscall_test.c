#include "syscall_test.h"
#include "syscall.h"

#include "../exception_vector_table.h"
#include "../console.h"
#include "../assert.h"

#define SYSCALL_TEST_RETURN_VALUE 0xDEADBEEFULL
#define SYSCALL_TEST_ARG0 0x12345678
#define SYSCALL_TEST_ARG1 0x87654321
#define SYSCALL_TEST_ARG2 0x11223344
#define SYSCALL_TEST_ARG3 0x44332211
#define SYSCALL_TEST_ARG4 0x88776655
#define SYSCALL_TEST_ARG5 0x55667788

void syscall_test(void) {
    uint64_t return_value = syscall_call(
        SYSCALL_TEST,
        SYSCALL_TEST_ARG0,
        SYSCALL_TEST_ARG1,
        SYSCALL_TEST_ARG2,
        SYSCALL_TEST_ARG3,
        SYSCALL_TEST_ARG4,
        SYSCALL_TEST_ARG5
    );

    if (return_value != SYSCALL_TEST_RETURN_VALUE) {
        console_write("SYSCALL_TEST returned incorrect value\r\n");
        assert(0);
    }
}

uint64_t syscall_test_impl(exception_frame_t *frame) {
    if (frame->registers[0] != SYSCALL_TEST_ARG0) {
        console_write("SYSCALL_TEST_ARG0 returned incorrect value\r\n");
        assert(0);
    }
    if (frame->registers[1] != SYSCALL_TEST_ARG1) {
        console_write("SYSCALL_TEST_ARG1 returned incorrect value\r\n");
        assert(0);
    }
    if (frame->registers[2] != SYSCALL_TEST_ARG2) {
        console_write("SYSCALL_TEST_ARG2 returned incorrect value\r\n");
        assert(0);
    }
    if (frame->registers[3] != SYSCALL_TEST_ARG3) {
        console_write("SYSCALL_TEST_ARG3 returned incorrect value\r\n");
        assert(0);
    }
    if (frame->registers[4] != SYSCALL_TEST_ARG4) {
        console_write("SYSCALL_TEST_ARG4 returned incorrect value\r\n");
        assert(0);
    }
    if (frame->registers[5] != SYSCALL_TEST_ARG5) {
        console_write("SYSCALL_TEST_ARG5 returned incorrect value\r\n");
        assert(0);
    }

    return SYSCALL_TEST_RETURN_VALUE;
}
