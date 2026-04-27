#include "time.h"

static inline uint64_t read_cntvct(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

static inline uint64_t read_cntfrq(void) {
    uint64_t val;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

uint64_t get_time_ns(void) {
    uint64_t ticks = read_cntvct();
    uint64_t freq  = read_cntfrq();

    return (ticks * 1000000000ULL) / freq;
}

uint64_t get_time_ms(void) {
    return get_time_ns() / 1000000ULL;
}
