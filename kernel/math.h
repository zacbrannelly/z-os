#pragma once

#include <stdint.h>

inline uint64_t math_max(uint64_t a, uint64_t b) {
    return a > b ? a : b;
}

inline uint64_t math_min(uint64_t a, uint64_t b) {
    return a < b ? a : b;
}

inline uint64_t math_clamp(uint64_t value, uint64_t min, uint64_t max) {
    return math_max(math_min(value, max), min);
}

inline int32_t math_max_int32(int32_t a, int32_t b) {
    return a > b ? a : b;
}

inline int32_t math_min_int32(int32_t a, int32_t b) {
    return a < b ? a : b;
}

inline int32_t math_clamp_int32(int32_t value, int32_t min, int32_t max) {
    return math_max_int32(math_min_int32(value, max), min);
}
