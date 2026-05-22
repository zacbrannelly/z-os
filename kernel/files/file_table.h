#pragma once

#include "file.h"

int file_table_init(void);
int file_table_destroy(void);

int file_table_open(const char* path, file_t file, handle_t *handle);
int file_table_close(handle_t handle);

int file_table_get(handle_t handle, file_t **file);
int file_table_get_by_path(const char *path, file_t **file);
