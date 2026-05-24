#include "psci.h"

#define PSCI_SYSTEM_OFF 0x84000008

int psci_shutdown(void) {
    register uint64_t func_id __asm__("x0") = PSCI_SYSTEM_OFF;
    __asm__ volatile(
        "hvc #0"
        : "+r"(func_id)
        :
        : "memory"
    );
    // If the system is still running after this, the shutdown failed.
    return -1;
}
