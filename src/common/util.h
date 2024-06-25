#pragma once

#include "defines.h"

INLINE const char *get_size_unit(u64 size, f32 *out_value)
{
    if (size >= GiB(1)) {
        *out_value = (f64)size / GiB(1);
        return "GiB";
    } else if (size >= MiB(1)) {
        *out_value = (f64)size / MiB(1);
        return "MiB";
    } else if (size >= KiB(1)) {
        *out_value = (f64)size / KiB(1);
        return "KiB";
    } else {
        *out_value = (f32)size;
        return "B";
    }
}
