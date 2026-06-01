#include "kernel.h"
#include "drivers/acpi/acpi.h"
#include "drivers/pci/pcie.h"
#include "drivers/pci/xhci.h"
#include "drivers/usb/usb_core.h"
#include "drivers/usb/usb_hid_mouse.h"
#include "drivers/usb/usb_hid_keyboard.h"
#include "drivers/uart/pl011.h"
#include "drivers/uart/uart_console.h"
#include "input/input.h"
#include "gfx/gfx.h"
#include "page_alloc.h"
#include "mmap.h"
#include "kmalloc.h"
#include "memory.h"
#include "time.h"
#include "assert.h"
#include "exception_vector_table.h"
#include "scheduler/scheduler.h"
#include "process/process.h"
#include "files/file_table.h"

#include <libz/string.h>
#include <libz/syscall.h>
#include <libinput/keycodes.h>
#include <stddef.h>

// TODO: Get these from the bootloader.
static const uint64_t pl011_base_address = 0x09000000;
static const uint64_t pl011_base_clock = 0x16e3600; // 24 MHz

static boot_info_t g_boot_info;

static void input_kernel_thread_entry(void) {
    while (1) {
        // Poll the USB devices for events.
        xhci_poll_events();
        usb_hid_mouse_poll();
        usb_hid_keyboard_poll();

        // Yield control to the scheduler.
        syscall_yield();
    }

    __builtin_unreachable();
}

void kernel_main(boot_info_t *boot_info) {
    // Make a copy of the boot info in the kernel stack.
    memory_copy(&g_boot_info, boot_info, sizeof(boot_info_t));
    boot_info = &g_boot_info;

    // Initialize virtual memory mapping system.
    assert(mmap_init(
        (efi_memory_descriptor_t *)boot_info->memory_map,
        boot_info->memory_map_size,
        boot_info->memory_map_descriptor_size
    ) == 0);

    // Initialize the exception vector table.
    assert(exception_vector_table_init() == 0);

    // Initialize the serial port.
    pl011_driver_t serial;
    assert(pl011_init(&serial, pl011_base_address, pl011_base_clock) == 0);

    // Initialize the console.
    console_t console;
    assert(uart_console_init(&console, &serial) == 0);
    console_set_active(&console);

    // Get the memory map (contains what physical memory is usable and what is reserved).
    mmap_memory_descriptor_t *memory_map = NULL;
    uint64_t memory_map_count = 0;
    assert(mmap_get_memory_map(&memory_map, &memory_map_count) == 0);

    // Initialize a physical memory page allocator.
    assert(page_alloc_init(memory_map, memory_map_count) == 0);
    
    // Initialize the kernel heap.
    assert(kernel_heap_init() == 0);

    // Initialize the file table.
    assert(file_table_init() == 0);

    // Initialize the graphics system.
    assert(gfx_init(boot_info) == 0);

    // Initialize the ACPI system.
    assert(acpi_init(boot_info->acpi_table) == 0);

    // Get the MCFG entry.
    acpi_table_mcfg_entry_t *mcfg_entry = acpi_get_mcfg_entry();
    assert(mcfg_entry != 0);

    // Initialize the PCIe system.
    assert(pcie_init(mcfg_entry) == 0);

    // Initialize the xHCI system.
    assert(xhci_init() == 0);

    // Initialize the USB system.
    assert(usb_init() == 0);

    // Initialize the USB HID mouse driver.
    assert(usb_hid_mouse_init() == 0);

    // Initialize the USB HID keyboard driver.
    assert(usb_hid_keyboard_init() == 0);

    // Bind the USB drivers to the USB devices.
    assert(usb_parse_interfaces() == 0);

    // Initialize the scheduler.
    assert(scheduler_init() == 0);

    // Initialize the input system.
    assert(input_init() == 0);

#if RUN_TESTS == 0
    // Schedule an input kernel thread (polls the USB devices for events).
    uint64_t input_kernel_thread_stack = (uint64_t)kmalloc(4096);
    thread_t input_kernel_thread;
    assert(thread_init(&input_kernel_thread, (uint64_t)input_kernel_thread_entry, input_kernel_thread_stack + 4096, THREAD_TYPE_KERNEL) == 0);
    assert(thread_start(&input_kernel_thread) == 0);
#endif

    for (int i = 0; i < boot_info->num_boot_modules; i++) {
        boot_module_t *boot_module = &boot_info->boot_modules[i];
        process_t *process = (process_t *)kmalloc(sizeof(process_t));

        assert(process != NULL);
        assert(process_init(process) == 0);

        uint64_t virtual_address = 0;
        assert(mmap_physical_to_virtual((uint64_t)boot_module->elf_buffer, &virtual_address) == 0);
        assert(process_load_elf(process, (uint8_t *)virtual_address, boot_module->elf_size) == 0);

        console_write("Loaded boot module: ");
        console_write(boot_module->name);
        console_write("\r\n");

        assert(process_start(process) == 0);
    }

    // Run the scheduler.
    scheduler_run();
    __builtin_unreachable();
}
