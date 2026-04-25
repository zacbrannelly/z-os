#include "kernel.h"
#include "format.h"
#include "drivers/acpi/acpi.h"
#include "drivers/pci/pcie.h"
#include "drivers/pci/xhci.h"
#include "drivers/usb/usb_core.h"
#include "drivers/usb/usb_hid_mouse.h"
#include "drivers/uart/pl011.h"
#include "drivers/uart/uart_console.h"
#include "string.h"
#include "page_alloc.h"
#include "mmap.h"

#include <stddef.h>

static const uint32_t clr_white = 0x00FFFFFF;

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
    // Clear the screen
    clear_screen(boot_info, clr_white);

    // Initialize the serial port.
    pl011_driver_t serial;
    pl011_init(&serial, pl011_base_address, pl011_base_clock);

    // Initialize the console.
    console_t console;
    uart_console_init(&console, &serial);
    console_set_active(&console);

    // Initialize virtual memory mapping system.
    mmap_init(
        (efi_memory_descriptor_t *)boot_info->memory_map,
        boot_info->memory_map_size,
        boot_info->memory_map_descriptor_size
    );

    // Get the memory map (contains what physical memory is usable and what is reserved).
    mmap_memory_descriptor_t *memory_map = NULL;
    uint64_t memory_map_count = 0;
    if (mmap_get_memory_map(&memory_map, &memory_map_count) < 0) {
        console_write("Failed to get memory map\r\n");
        return;
    }

    // Initialize a physical memory page allocator.
    page_alloc_init(memory_map, memory_map_count);

    // Print the banner.
    console_write(g_banner);

    console_write("Testing format_hex: ");
    char buffer[17];
    console_write("0x");
    format_hex(buffer, sizeof(buffer), 0x1234ef);
    console_write(buffer);
    console_write("\r\n");

    acpi_init(boot_info->acpi_table);
    acpi_table_mcfg_entry_t *mcfg_entry = acpi_get_mcfg_entry();
    if (mcfg_entry == 0) {
        console_write("Failed to get MCFG entry\r\n");
        return;
    }

    if (pcie_init(mcfg_entry) < 0) {
        console_write("Failed to initialize PCIe\r\n");
        return;
    }
    
    if (xhci_init() < 0) {
        console_write("Failed to initialize xHCI\r\n");
        return;
    }

    if (usb_init() < 0) {
        console_write("Failed to initialize USB\r\n");
        return;
    }

    if (usb_hid_mouse_init() < 0) {
        console_write("Failed to initialize USB HID mouse driver\r\n");
        return;
    }

    if (usb_parse_interfaces() < 0) {
        console_write("Failed to parse USB interfaces\r\n");
        return;
    }

    while (1) {
        xhci_poll_events();
        usb_hid_mouse_poll();

        usb_hid_mouse_report_t *report = usb_hid_mouse_get_report();
        if (report != NULL) {
            // Set the colour of the screen based on the report.
            uint32_t color = 0x00000000;
            if (report->buttons & 0x01) {
                color |= 0x000000FF;
            }
            if (report->buttons & 0x02) {
                color |= 0x0000FF00;
            }
            if (report->buttons & 0x04) {
                color |= 0x00FF0000;
            }
            clear_screen(boot_info, color);
        }
    }

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

    // Spin forever
    while (1) {
        __asm__ volatile("wfe");
    }
}
