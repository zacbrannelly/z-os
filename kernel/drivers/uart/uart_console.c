#include "uart_console.h"

int uart_console_init(console_t *console, pl011_driver_t *serial) {
    console->putc = (void (*)(const void *data, char c))pl011_send_char;
    console->getc = (char (*)(const void *data))pl011_receive_char;
    console->data = serial;
    return 0;
}
