#include "maths.h"

#include <math.h>
#include <time.h>
#include <stdlib.h>

static b8 rand_seeded = false;

i32 maths_random(void)
{
    if (!rand_seeded) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        srand((u32)ts.tv_nsec);
        rand_seeded = true;
    }

    return rand();
}

i32 maths_random_range(i32 min, i32 max)
{
    return min + (maths_random() % (max - min));
}

f32 maths_frandom(void)
{
    return (f32)maths_random() / (f32)RAND_MAX;
}

f32 maths_frandom_range(f32 min, f32 max)
{
    return min + ((f32)maths_random() / ((f32)RAND_MAX / (max - min)));
}
