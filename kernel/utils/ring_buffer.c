#include "ring_buffer.h"

#include "../memory.h"

static void ring_buffer_advance_ptr(ring_buffer_t *ring_buffer, uint64_t *ptr) {
    *ptr += ring_buffer->data_size;

    if (*ptr >= ring_buffer->base_address + ring_buffer->ring_size * ring_buffer->data_size) {
        *ptr = ring_buffer->base_address;
    }
}

int ring_buffer_init(
    ring_buffer_t *ring_buffer,
    uint64_t base_address,
    uint64_t ring_size,
    uint64_t data_size
) {
    if (ring_buffer == 0 || data_size == 0 || ring_size == 0) {
        return -1;
    }

    ring_buffer->base_address = base_address;
    ring_buffer->ring_size = ring_size;
    ring_buffer->data_size = data_size;
    ring_buffer->enqueue_ptr = base_address;
    ring_buffer->dequeue_ptr = base_address;
    ring_buffer->used_count = 0;
    return 0;
}

int ring_buffer_enqueue(ring_buffer_t *ring_buffer, void *data, uint64_t data_size) {
    if (ring_buffer == 0 || data == 0 || data_size > ring_buffer->data_size) {
        return -1;
    }

    if (ring_buffer_is_full(ring_buffer)) {
        return -1;
    }

    memory_copy((void *)ring_buffer->enqueue_ptr, data, data_size);
    ring_buffer_advance_ptr(ring_buffer, &ring_buffer->enqueue_ptr);
    ring_buffer->used_count++;

    return 0;
}

int ring_buffer_dequeue(ring_buffer_t *ring_buffer, void *data, uint64_t data_size) {
    if (ring_buffer == 0 || data == 0 || data_size > ring_buffer->data_size) {
        return -1;
    }

    if (ring_buffer_is_empty(ring_buffer)) {
        return -1;
    }

    memory_copy(data, (void *)ring_buffer->dequeue_ptr, data_size);
    ring_buffer_advance_ptr(ring_buffer, &ring_buffer->dequeue_ptr);
    ring_buffer->used_count--;

    return 0;
}

int ring_buffer_peek(ring_buffer_t *ring_buffer, void *data, uint64_t data_size) {
    if (ring_buffer == 0 || data == 0 || data_size > ring_buffer->data_size) {
        return -1;
    }

    if (ring_buffer_is_empty(ring_buffer)) {
        return -1;
    }

    memory_copy(data, (void *)ring_buffer->dequeue_ptr, data_size);

    return 0;
}

uint8_t ring_buffer_is_empty(ring_buffer_t *ring_buffer) {
    return ring_buffer == 0 || ring_buffer->used_count == 0;
}

uint8_t ring_buffer_is_full(ring_buffer_t *ring_buffer) {
    return ring_buffer != 0
        && ring_buffer->data_size != 0
        && ring_buffer->used_count == ring_buffer->ring_size;
}
