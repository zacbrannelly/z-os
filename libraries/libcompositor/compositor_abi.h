#pragma once

#include <stdint.h>
#include <libz/handle.h>

#define COMPOSITOR_ABI_CHANNEL_PATH "/compositor/channel/0"

typedef enum compositor_abi_type_t {
    COMPOSITOR_ABI_TYPE_CREATE_WINDOW_REQUEST,
    COMPOSITOR_ABI_TYPE_CREATE_WINDOW_RESPONSE,
} compositor_abi_type_t;

typedef enum compositor_abi_status_t {
    COMPOSITOR_ABI_STATUS_SUCCESS,
    COMPOSITOR_ABI_STATUS_ERROR,
} compositor_abi_status_t;

typedef struct compositor_abi_create_window_payload_t {
    char name[256];
    uint64_t width;
    uint64_t height;
} compositor_abi_create_window_payload_t;

typedef struct compositor_abi_create_window_response_payload_t {
    compositor_abi_status_t status;
    handle_t window_handle;
} compositor_abi_create_window_response_payload_t;

typedef struct compositor_abi_payload_t {
    compositor_abi_type_t type;
    handle_t process_handle;
    union {
        compositor_abi_create_window_payload_t create_window;
        compositor_abi_create_window_response_payload_t create_window_response;
    } payload;
} compositor_abi_payload_t;
