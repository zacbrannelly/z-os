#include "kernel.h"

void clear_screen(boot_info_t *boot_info, uint32_t color) {
    // Clear the screen
    for (int i = 0; i < boot_info->framebuffer_size; i++) {
        boot_info->framebuffer[i] = color;
    }
}

void kernel_main(boot_info_t *boot_info) {
    // Clear the screen (red)
    clear_screen(boot_info, 0x00FF0000);

    // Spin forever
    while (1);    
}
