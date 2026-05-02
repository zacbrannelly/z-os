#include "kernel.h"
#include "format.h"
#include "drivers/acpi/acpi.h"
#include "drivers/pci/pcie.h"
#include "drivers/pci/xhci.h"
#include "drivers/usb/usb_core.h"
#include "drivers/usb/usb_hid_mouse.h"
#include "drivers/usb/usb_hid_keyboard.h"
#include "drivers/uart/pl011.h"
#include "drivers/uart/uart_console.h"
#include "gfx/gfx.h"
#include "gfx/font.h"
#include "ui/cursor.h"
#include "ui/text_input.h"
#include "string.h"
#include "page_alloc.h"
#include "mmap.h"
#include "kmalloc.h"
#include "memory.h"
#include "time.h"
#include "exception_vector_table.h"

#include <stddef.h>

// TODO: Get these from the bootloader.
static const uint64_t pl011_base_address = 0x09000000;
static const uint64_t pl011_base_clock = 0x16e3600; // 24 MHz

void kernel_main(boot_info_t *boot_info) {
    // Initialize the serial port.
    pl011_driver_t serial;
    if (pl011_init(&serial, pl011_base_address, pl011_base_clock) < 0) {
        console_write("Failed to initialize PL011 UART\r\n");
        return;
    }

    // Initialize the console.
    console_t console;
    if (uart_console_init(&console, &serial) < 0) {
        console_write("Failed to initialize UART console\r\n");
        return;
    }
    console_set_active(&console);

    // Initialize the exception vector table.
    if (exception_vector_table_init() < 0) {
        console_write("Failed to initialize exception vector table\r\n");
        return;
    }

    // Initialize virtual memory mapping system.
    if (mmap_init(
        (efi_memory_descriptor_t *)boot_info->memory_map,
        boot_info->memory_map_size,
        boot_info->memory_map_descriptor_size
    ) < 0) {
        console_write("Failed to initialize virtual memory mapping system\r\n");
        return;
    }

    // Get the memory map (contains what physical memory is usable and what is reserved).
    mmap_memory_descriptor_t *memory_map = NULL;
    uint64_t memory_map_count = 0;
    if (mmap_get_memory_map(&memory_map, &memory_map_count) < 0) {
        console_write("Failed to get memory map\r\n");
        return;
    }

    // Initialize a physical memory page allocator.
    if (page_alloc_init(memory_map, memory_map_count) < 0) {
        console_write("Failed to initialize physical memory page allocator\r\n");
        return;
    }
    
    // Initialize the kernel heap.
    if (kernel_heap_init() < 0) {
        console_write("Failed to initialize kernel heap\r\n");
        return;
    }

    // Initialize the graphics system.
    if (gfx_init(boot_info) < 0) {
        console_write("Failed to initialize graphics\r\n");
        return;
    }

    uint32_t framebuffer_width = gfx_get_framebuffer_width();
    uint32_t framebuffer_height = gfx_get_framebuffer_height();

    if (cursor_init(framebuffer_width, framebuffer_height) < 0) {
        console_write("Failed to initialize cursor\r\n");
        return;
    }

    if (acpi_init(boot_info->acpi_table) < 0) {
        console_write("Failed to initialize ACPI\r\n");
        return;
    }

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

    if (usb_hid_keyboard_init() < 0) {
        console_write("Failed to initialize USB HID keyboard driver\r\n");
        return;
    }

    if (usb_parse_interfaces() < 0) {
        console_write("Failed to parse USB interfaces\r\n");
        return;
    }

    text_input_t text_input;
    if (text_input_alloc(&text_input) < 0) {
        console_write("Failed to initialize text input\r\n");
        return;
    }

    while (1) {
        gfx_clear(GFX_COLOR_BLACK);

        xhci_poll_events();
        usb_hid_mouse_poll();
        usb_hid_keyboard_poll();
        cursor_update();

        text_input_draw(&text_input);
        cursor_draw();
        gfx_swap_buffers();
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
