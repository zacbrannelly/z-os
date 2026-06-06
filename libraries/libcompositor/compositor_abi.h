#pragma once

#include <stdint.h>
#include <libz/handle.h>

#define COMPOSITOR_ABI_CHANNEL_PATH "/compositor/channel/0"

typedef enum compositor_abi_type_t {
    COMPOSITOR_ABI_TYPE_CREATE_WINDOW_REQUEST,
    COMPOSITOR_ABI_TYPE_CREATE_WINDOW_RESPONSE,
    COMPOSITOR_ABI_TYPE_ATTACH_BUFFER_REQUEST,
    COMPOSITOR_ABI_TYPE_ATTACH_BUFFER_RESPONSE,
    COMPOSITOR_ABI_TYPE_COMMIT_REQUEST,
    COMPOSITOR_ABI_TYPE_COMMIT_RESPONSE,
} compositor_abi_type_t;

typedef enum compositor_abi_status_t {
    COMPOSITOR_ABI_STATUS_SUCCESS,
    COMPOSITOR_ABI_STATUS_ERROR,
} compositor_abi_status_t;

typedef struct compositor_abi_status_response_payload_t {
    compositor_abi_status_t status;
} compositor_abi_status_response_payload_t;

/**
 * CREATE_WINDOW command.
 */
typedef struct compositor_abi_create_window_payload_t {
    char name[256];
    uint64_t width;
    uint64_t height;
} compositor_abi_create_window_payload_t;

typedef struct compositor_abi_create_window_response_payload_t {
    compositor_abi_status_t status;
    handle_t window_handle;
} compositor_abi_create_window_response_payload_t;

/**
 * ATTACH_BUFFER command.
 */
typedef struct compositor_abi_attach_buffer_payload_t {
    handle_t window_handle;
    handle_t buffer_handle;
} compositor_abi_attach_buffer_payload_t;

/**
 * COMMIT command.
 */
typedef struct compositor_abi_commit_payload_t {
    handle_t window_handle;
    uint64_t x;
    uint64_t y;
    uint64_t width;
    uint64_t height;
} compositor_abi_commit_payload_t;

/**
 * Common payload for all commands.
 */
typedef struct compositor_abi_payload_t {
    compositor_abi_type_t type;
    handle_t process_handle;
    union {
        // CREATE_WINDOW command.
        compositor_abi_create_window_payload_t create_window;
        compositor_abi_create_window_response_payload_t create_window_response;

        // ATTACH_BUFFER command.
        compositor_abi_attach_buffer_payload_t attach_buffer;
        compositor_abi_status_response_payload_t attach_buffer_response;

        // COMMIT command.
        compositor_abi_commit_payload_t commit;
        compositor_abi_status_response_payload_t commit_response;
    } payload;
} compositor_abi_payload_t;
