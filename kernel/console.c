#include "console.h"
#include "format.h"
#include <stddef.h>

static console_t *g_console = NULL;

void console_set_active(console_t *console) {
    g_console = console;
}

void console_putc(char c) {
    if (g_console == NULL) {
        return;
    }
    g_console->putc(g_console->data, c);
}

void console_write(const char *s) {
    if (g_console == NULL) {
        return;
    }

    while (1) {
        if (*s == '\0') {
            break;
        }
        g_console->putc(g_console->data, *s);
        s++;
    }
}

void console_write_hex(uint64_t value) {
    if (g_console == NULL) {
        return;
    }
    char buffer[17];
    console_write("0x");
    format_hex(buffer, sizeof(buffer), value);
    console_write(buffer);
}

void console_write_chars(const char *s, uint64_t count) {
    if (g_console == NULL) {
        return;
    }
    for (uint64_t i = 0; i < count; i++) {
        g_console->putc(g_console->data, s[i]);
    }
}

int console_getc(void) {
    if (g_console == NULL) {
        return -1;
    }
    return g_console->getc(g_console->data);
}

int console_try_getc(void) {
    if (g_console == NULL) {
        return -1;
    }
    return g_console->try_getc(g_console->data);
}
