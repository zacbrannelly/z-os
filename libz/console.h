#pragma once

#include <stdint.h>

// Transmission
void console_putc(char c);
void console_write(const char *s);
void console_write_hex(uint64_t value);
void console_write_chars(const char *s, uint64_t count);

// Reception
// TODO: int console_getc(void);
// TODO: int console_try_getc(void);
