#include "assert.h"
#include "drivers/psci/psci.h"

void assert_impl(int condition, const char *message) {
    if (condition) return;
    
    console_write("Assertion failed: ");
    console_write(message);
    console_write("\r\n");
#if RUN_TESTS == 1
    psci_shutdown();
    __builtin_unreachable();
#else
    while (1) {
        __asm__ volatile("wfe");
    }
#endif
}
