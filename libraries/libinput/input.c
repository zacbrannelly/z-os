#include "input.h"

#include <stddef.h>
#include <libz/file.h>

int input_open(const char *path, handle_t *fd) {
    if (path == NULL || fd == NULL) {
        return -1;
    }

    // TODO: Validate that the path is a valid input device path.
    // TODO: Unsure what this mechanism should be.

    return open(path, fd, 0);
}

int input_open_nonblock(const char *path, handle_t *fd) {
    if (path == NULL || fd == NULL) {
        return -1;
    }

    // TODO: Validate that the path is a valid input device path.

    return open(path, fd, O_NONBLOCK);
}

int input_read(handle_t fd, input_device_event_t *event) {
    if (fd < 0 || event == NULL) {
        return -1;
    }

    return read(fd, (void *)event, sizeof(input_device_event_t));
}
