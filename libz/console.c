#include "console.h"
#include "format.h"
#include "syscall.h"
#include <stddef.h>

void console_putc(char c) {
    char buffer[2] = { c, '\0' };
    syscall_console_write(buffer);
}

void console_write(const char *s) {
    syscall_console_write(s);
}

void console_write_hex(uint64_t value) {
    char buffer[17];
    console_write("0x");
    format_hex(buffer, sizeof(buffer), value);
    console_write(buffer);
}

void console_write_chars(const char *s, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        console_putc(s[i]);
    }
}

// TODO: int console_getc(void);
// TODO: int console_try_getc(void);
