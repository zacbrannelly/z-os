#pragma once

#include "../../console.h"
#include "pl011.h"

int uart_console_init(console_t *console, pl011_driver_t *serial);
