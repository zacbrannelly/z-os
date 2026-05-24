#include "assert.h"
#include "syscall.h"

void assert_impl(int condition, const char *message) {
    if (condition) return;
    
    syscall_console_write("Assertion failed: ");
    syscall_console_write(message);
    syscall_console_write("\r\n");
#if RUN_TESTS == 1
    syscall_shutdown();
    __builtin_unreachable();
#else
    while (1) {
        __asm__ volatile("wfe");
    }
#endif
}
