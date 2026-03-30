#include "console.h"
#include <stddef.h>

static console_t *g_console = NULL;

void console_set_active(console_t *console) {
    g_console = console;
}

void console_putc(char c) {
    g_console->putc(g_console->data, c);
}

void console_write(const char *s) {
    while (1) {
        if (*s == '\0') {
            break;
        }
        g_console->putc(g_console->data, *s);
        s++;
    }
}

int console_getc(void) {
    return g_console->getc(g_console->data);
}

int console_try_getc(void) {
    return g_console->try_getc(g_console->data);
}
