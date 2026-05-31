#pragma once

#include <libz/handle.h>
#include "input_device_event.h"

int input_open(const char *path, handle_t *fd);
int input_open_nonblock(const char *path, handle_t *fd);
int input_read(handle_t fd, input_device_event_t *event);
