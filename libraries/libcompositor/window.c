#include "window.h"

#include <stddef.h>
#include <libz/assert.h>
#include <libz/memory.h>
#include <libz/console.h>
#include <libz/string.h>
#include "comms.h"
#include "compositor_abi.h"

int window_create(
    const char *name,
    uint64_t width,
    uint64_t height,
    handle_t *window_handle
) {
    if (name == NULL || window_handle == NULL) {
        return -1;
    }

    compositor_abi_payload_t message;
    memory_set(&message, 0, sizeof(compositor_abi_payload_t));

    compositor_abi_payload_t response;
    memory_set(&response, 0, sizeof(compositor_abi_payload_t));

    message.type = COMPOSITOR_ABI_TYPE_CREATE_WINDOW_REQUEST;
    memory_copy(&message.payload.create_window.name[0], (char *)name, strlen(name) + 1);
    message.payload.create_window.width = width;
    message.payload.create_window.height = height;

    if (comms_send_request(&message, &response) < 0) {
        return -1;
    }

    assert(response.type == COMPOSITOR_ABI_TYPE_CREATE_WINDOW_RESPONSE);
    if (response.payload.create_window_response.status != COMPOSITOR_ABI_STATUS_SUCCESS) {
        console_write("window_create: failed to create window\r\n");
        return -1;
    }

    *window_handle = response.payload.create_window_response.window_handle;
    return 0;
}
