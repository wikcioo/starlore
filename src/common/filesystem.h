#pragma once

#include "defines.h"

typedef struct file_handle {
    void *handle;
    b8 is_valid  : 1;
    b8 is_binary : 1;
    b8 modes     : 3;
} file_handle_t;

typedef enum file_mode {
    FILE_MODE_READ   = BIT(0),
    FILE_MODE_WRITE  = BIT(1),
    FILE_MODE_APPEND = BIT(2)
} file_mode_e;

b8 filesystem_open(const char *path, file_mode_e mode, b8 is_binary, file_handle_t *out_handle);
void filesystem_close(file_handle_t *handle);

b8 filesystem_exists(const char *path);
void filesystem_get_size(const file_handle_t *handle, u64 *out_size);

b8 filesystem_read_all(const file_handle_t *handle, void *buffer, u64 *out_bytes_read);
