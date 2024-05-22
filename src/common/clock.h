#pragma once

#include "defines.h"

#include <time.h>

INLINE u64 clock_get_absolute_time_ns(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1000000000 + now.tv_nsec;
}
