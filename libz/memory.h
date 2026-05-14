#pragma once

#include <stdint.h>

void memory_set(void *address, uint8_t value, uint64_t size);
void memory_copy(void *destination, void *source, uint64_t size);
