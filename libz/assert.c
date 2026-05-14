#include "assert.h"
#include "syscall.h"

void assert_impl(int condition, const char *message) {
    if (condition) return;
    
    syscall_console_write("Assertion failed: ");
    syscall_console_write(message);
    syscall_console_write("\r\n");
    while (1) {
        __asm__ volatile("wfe");
    }
}
