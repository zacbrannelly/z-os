#include "kernel.h"
#include "drivers/uart/pl011.h"
#include "drivers/uart/uart_console.h"

static const uint32_t clr_red = 0x00FF0000;

// TODO: Get these from the bootloader.
static const uint64_t pl011_base_address = 0x09000000;
static const uint64_t pl011_base_clock = 0x16e3600; // 24 MHz

uint32_t g_counter = 0x00ffffff;
const char g_banner[] = "\x1b[31mHello, ANSI world!\x1b[0m\r\n";

void clear_screen(boot_info_t *boot_info, uint32_t color) {
    // Clear the screen
    for (int i = 0; i < boot_info->framebuffer_size; i++) {
        boot_info->framebuffer[i] = color;
    }
}

void kernel_main(boot_info_t *boot_info) {
    pl011_driver_t serial;
    pl011_init(&serial, pl011_base_address, pl011_base_clock);

    console_t console;
    uart_console_init(&console, &serial);
    console_set_active(&console);

    // Print the banner.
    console_write(g_banner);

    while (1) {
        // Wait for a character to be received.
        char c = console_getc();
        if (c == 'q') {
            break;
        }

        // Newline or carriage return.
        if (c == '\r' || c == '\n') {
            console_write("\r\n");
            continue;
        }

        // Backspace or delete.
        if (c == '\b' || c == 0x7f) {
            console_write("\b \b");
            continue;
        }

        // Echo the character back to the serial port.
        console_putc(c);
    }

    // Clear the screen (red)
    clear_screen(boot_info, g_counter);

    // Spin forever
    while (1) {
        __asm__ volatile("wfe");
    }
}
