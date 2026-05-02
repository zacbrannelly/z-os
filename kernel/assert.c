#include "assert.h"

void assert_impl(int condition, const char *message) {
    if (condition) return;
    
    console_write("Assertion failed: ");
    console_write(message);
    console_write("\r\n");
    while (1) {
        __asm__ volatile("wfe");
    }
}
