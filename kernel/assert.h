#pragma once

#include <stdint.h>
#include "console.h"

#define assert(condition) assert_impl(condition, #condition)

void assert_impl(int condition, const char *message);
