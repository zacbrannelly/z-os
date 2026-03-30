#pragma once

typedef struct console_t {
    void (*putc)(const void *data, char c);
    char (*getc)(const void *data);
    char (*try_getc)(const void *data);

    void *data;
} console_t;

void console_set_active(console_t *console);

// Transmission
void console_putc(char c);
void console_write(const char *s);

// Reception
int console_getc(void);
int console_try_getc(void);
