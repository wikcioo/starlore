#include "maths.h"

#include <math.h>
#include <time.h>
#include <stdlib.h>

static b8 rand_seeded = false;

f32 math_sinf(f32 value)
{
    return sinf(value);
}

f32 math_cosf(f32 value)
{
    return cosf(value);
}

i32 math_random(void)
{
    if (!rand_seeded) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        srand((u32)ts.tv_nsec);
        rand_seeded = true;
    }

    return rand();
}

i32 math_random_range(i32 min, i32 max)
{
    return min + (math_random() % (max - min));
}

f32 math_frandom(void)
{
    return (f32)math_random() / (f32)RAND_MAX;
}

f32 math_frandom_range(f32 min, f32 max)
{
    return min + ((f32)math_random() / ((f32)RAND_MAX / (max - min)));
}
