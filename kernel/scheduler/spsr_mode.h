#pragma once

#define SPSR_MODE_EL0   0x0 // User mode
#define SPSR_MODE_EL1_T 0x4 // Kernel mode (but use user stack)
#define SPSR_MODE_EL1_H 0x5 // Kernel mode (use kernel stack)
