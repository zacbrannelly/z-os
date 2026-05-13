#include "exception_vector_table.h"

#include "console.h"
#include "assert.h"
#include "syscall/syscall_handler.h"

// Exception class codes
#define SVC_EXCEPTION_CLASS 0x15

#define EXCEPTION_CLASS(esr) ((esr >> 26) & 0x3F)

static void write_vbar_el1(uint64_t vbar_el1) {
    __asm__("msr vbar_el1, %0" :: "r" (vbar_el1) : "memory");
    __asm__("isb");
}

int exception_vector_table_init(void) {
    // Write the exception vector table to the VBAR_EL1 register.
    write_vbar_el1((uint64_t)exception_vector_table);
    return 0;
}

void handle_sync_exception(exception_frame_t *frame, uint64_t esr) {
    uint8_t exception_class = EXCEPTION_CLASS(esr);
    switch (exception_class) {
        case SVC_EXCEPTION_CLASS:
            syscall_handler(frame);
            break;
        default:
            console_write("Unknown exception occurred (class: ");
            console_write_hex(exception_class);
            console_write(")\r\n");
            assert(0);
    }
}
