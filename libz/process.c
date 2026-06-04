#include "process.h"
#include "syscall.h"

handle_t process_get_handle(void) {
    return syscall_process_get_handle();
}
