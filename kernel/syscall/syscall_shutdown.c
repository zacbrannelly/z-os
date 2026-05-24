#include "syscall_shm.h"

#include "../drivers/psci/psci.h"
#include <stddef.h>

uint64_t syscall_shutdown_impl(exception_frame_t *frame) {
    return psci_shutdown();
}
