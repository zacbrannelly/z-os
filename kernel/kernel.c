#include "kernel.h"

static const uint32_t CLR_RED = 0x00FF0000;

void clear_screen(boot_info_t *boot_info, uint32_t color) {
    // Clear the screen
    for (int i = 0; i < boot_info->framebuffer_size; i++) {
        boot_info->framebuffer[i] = color;
    }
}

void kernel_main(boot_info_t *boot_info) {
    // Clear the screen (red)
    clear_screen(boot_info, CLR_RED);

    // Spin forever
    while (1) {
        __asm__ volatile("wfe");
    }
}
