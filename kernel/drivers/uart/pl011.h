#pragma once

#include <stdint.h>

typedef struct pl011_driver_t {
    uint64_t base_address;
    uint64_t base_clock;
    uint32_t baud_rate;
    uint32_t data_bits;
    uint32_t stop_bits;
} pl011_driver_t;

int pl011_init(pl011_driver_t *driver, uint64_t base_address, uint64_t base_clock);
int pl011_reset(const pl011_driver_t *driver);

// TX - Transmission
int pl011_send_char(const pl011_driver_t *driver, char c);
int pl011_send(const pl011_driver_t *driver, const char *data, uint64_t size);

// RX - Reception
char pl011_receive_char(const pl011_driver_t *driver);
