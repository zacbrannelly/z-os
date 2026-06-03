#include "gfx.h"

#include <stddef.h>
#include <libz/handle.h>

#include "../assert.h"
#include "../kernel.h"
#include "../console.h"
#include "../process/shared_memory.h"

int gfx_init(boot_info_t *boot_info) {
    if (boot_info == NULL) {
        console_write("Failed to initialize graphics: boot info is NULL\r\n");
        return -1;
    }

    // Map the physical framebuffere to /dev/fb0 (for userland access).
    shared_memory_t *shared_memory = NULL;
    handle_t global_handle = 0;
    assert(shared_memory_create_from_contiguous_pages(
        "/dev/fb0",
        (uint64_t)boot_info->framebuffer,
        boot_info->framebuffer_size * sizeof(uint32_t),
        &shared_memory,
        &global_handle
    ) == 0);
    assert(shared_memory != NULL);
    assert(global_handle >= 0);

    return 0;
}
