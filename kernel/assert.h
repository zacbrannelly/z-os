#pragma once

#include <stdint.h>
#include "console.h"

#define assert(condition) assert_impl(condition, #condition)

inline void assert_impl(int condition, const char *message) {
    if (!condition) {
        console_write("Assertion failed: ");
        console_write(message);
        console_write("\r\n");
        while (1) {
            __asm__ volatile("wfe");
        }
    }
}
