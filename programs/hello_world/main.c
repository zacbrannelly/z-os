#include <stddef.h>
#include <libz/console.h>
#include <libz/syscall.h>
#include <libz/shm.h>
#include <libz/mmap.h>
#include <libgfx/bitmap.h>
#include <libgfx/colors.h>
#include <libgfx/paint.h>
#include <libgfx/font.h>
#include <libcompositor/window.h>

int main(void) {
    int width = 480;
    int height = 320;

    handle_t window_handle;
    if (window_create("Hello, Windows!", width, height, &window_handle) < 0) {
        console_write("hello_world: window_create failed\r\n");
        return 1;
    }
    console_write("hello_world: window_create succeeded\r\n");

    handle_t buffer_handle;
    uint64_t buffer_size = width * height * sizeof(uint32_t);
    if (shm_open("/hello_world/buffer", buffer_size, &buffer_handle) < 0) {
        console_write("hello_world: shm_open failed\r\n");
        return 1;
    }
    console_write("hello_world: shm_open succeeded\r\n");

    // Attach the buffer to the window.
    if (window_attach_buffer(window_handle, buffer_handle) < 0) {
        console_write("hello_world: window_attach_buffer failed\r\n");
        return 1;
    }
    console_write("hello_world: window_attach_buffer succeeded\r\n");

    // Map the buffer to the process's address space.
    uint32_t *buffer = (uint32_t *)mmap(0, buffer_size, MAP_SHARED | MAP_READ | MAP_WRITE, buffer_handle);
    if (buffer == NULL) {
        console_write("hello_world: mmap failed\r\n");
        return 1;
    }

    // Create a bitmap from the buffer.
    bitmap_t *window_buffer = bitmap_from_data((uint8_t *)buffer, width, height, BITMAP_PIXEL_FORMAT_RGB32);
    if (window_buffer == NULL) {
        console_write("hello_world: bitmap_from_data failed\r\n");
        return 1;
    }

    if (font_init() < 0) {
        console_write("hello_world: font_init failed\r\n");
        return 1;
    }
    console_write("hello_world: font_init succeeded\r\n");

    // Clear the buffer with a white color.
    paint_fill_rect(window_buffer, 0, 0, width, height, RGB_COLOR(255, 0, 0));
    font_draw_text_bitmap(window_buffer, "Hello, Windows!", 100, 100, RGB_COLOR_WHITE);

    // Apply the buffer to the window.
    if (window_commit(window_handle, 0, 0, width, height) < 0) {
        console_write("hello_world: window_commit failed\r\n");
        return 1;
    }
    console_write("hello_world: window_commit succeeded\r\n");

    while (1) {
        syscall_yield();
    }

    return 0;
}
