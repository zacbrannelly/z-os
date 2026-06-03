#include "window.h"

#include <stddef.h>
#include <libz/memory.h>
#include <libz/console.h>
#include <libcompositor/compositor_abi.h>

#include "../compositor.h"

int window_abi_create(
    compositor_t *compositor,
    compositor_abi_payload_t *message,
    compositor_abi_payload_t *response
) {
    if (compositor == NULL || message == NULL || response == NULL) {
        return -1;
    }

    handle_t window_handle;
    int result = compositor_window_create(
        message->payload.create_window.name,
        message->payload.create_window.width,
        message->payload.create_window.height,
        &window_handle
    );

    response->type = COMPOSITOR_ABI_TYPE_CREATE_WINDOW_RESPONSE;
    response->process_handle = message->process_handle;
    response->payload.create_window_response.status = result == 0 
        ? COMPOSITOR_ABI_STATUS_SUCCESS 
        : COMPOSITOR_ABI_STATUS_ERROR;
    response->payload.create_window_response.window_handle = window_handle;

    return 0;
}
