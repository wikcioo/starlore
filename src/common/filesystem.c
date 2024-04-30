#include "filesystem.h"

#include <stdio.h>
#include <sys/stat.h>

#include "logger.h"
#include "asserts.h"

b8 filesystem_open(const char *path, file_mode_e modes, b8 is_binary, file_handle_t *out_handle)
{
    ASSERT(path);
    ASSERT(out_handle);

    out_handle->handle = NULL;
    out_handle->is_valid = false;

    const char *modes_str = NULL;
    if (modes & FILE_MODE_READ) {
        modes_str = is_binary ? "rb" : "r";
    } else if (modes & FILE_MODE_WRITE) {
        modes_str = is_binary ? "wb" : "w";
    } else if (modes & FILE_MODE_APPEND) {
        modes_str = is_binary ? "ab" : "a";
    }

    ASSERT(modes_str);

    FILE *file = fopen(path, modes_str);
    if (file == NULL) {
        LOG_ERROR("failed to open file at %s", path);
        return false;
    }

    out_handle->handle = file;
    out_handle->is_valid = true;
    out_handle->is_binary = is_binary;
    out_handle->modes = modes;

    return true;
}

void filesystem_close(file_handle_t *handle)
{
    ASSERT(handle);

    handle->handle = NULL;
    handle->is_valid = false;
}

b8 filesystem_exists(const char *path)
{
    ASSERT(path);

    struct stat buffer;
    return stat(path, &buffer) == 0;
}

void filesystem_get_size(const file_handle_t *handle, u64 *out_size)
{
    ASSERT(handle);
    ASSERT(out_size);

    fseek((FILE *)handle->handle, 0, SEEK_END);
    *out_size = (u64)ftell((FILE *)handle->handle);
    fseek((FILE *)handle->handle, 0, SEEK_SET);
}

b8 filesystem_read_all(const file_handle_t *handle, void *buffer, u64 *out_bytes_read)
{
    ASSERT(handle);
    ASSERT(buffer);

    if (!(handle->modes & FILE_MODE_READ)) {
        LOG_ERROR("file not opened in read mode");
        return false;
    }

    u64 file_size;
    filesystem_get_size(handle, &file_size);

    u64 bytes_read = fread(buffer, 1, file_size, (FILE *)handle->handle);
    if (out_bytes_read) {
        *out_bytes_read = bytes_read;
    }

    return bytes_read == file_size;
}
