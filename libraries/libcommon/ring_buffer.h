#pragma once

#include <stdint.h>

typedef struct ring_buffer_t {
    uint64_t base_address;
    uint64_t enqueue_ptr;
    uint64_t dequeue_ptr;
    uint64_t ring_size;
    uint64_t data_size;
    uint64_t used_count;
} ring_buffer_t;

int ring_buffer_init(
    ring_buffer_t *ring_buffer,
    uint64_t base_address,
    uint64_t ring_size,
    uint64_t data_size
);
int ring_buffer_enqueue(ring_buffer_t *ring_buffer, void *data, uint64_t data_size);
int ring_buffer_dequeue(ring_buffer_t *ring_buffer, void *data, uint64_t data_size);
int ring_buffer_peek(ring_buffer_t *ring_buffer, void *data, uint64_t data_size);
uint8_t ring_buffer_is_empty(ring_buffer_t *ring_buffer);
uint8_t ring_buffer_is_full(ring_buffer_t *ring_buffer);
