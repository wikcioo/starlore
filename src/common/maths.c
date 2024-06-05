#include "maths.h"

#include <math.h>
#include <stdlib.h>

#include "common/clock.h"

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
        srand((u32)clock_get_absolute_time_ns());
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

f32 math_floor(f32 value)
{
    return floorf(value);
}

f32 math_ceil(f32 value)
{
    return ceilf(value);
}

f32 math_round(f32 value)
{
    return roundf(value);
}

vec2 vec2_normalize(vec2 vector)
{
    f32 length = sqrtf(vector.x * vector.x + vector.y * vector.y);
    if (length == 0.0f) {
        return (vec2) {
            .x = 0.0f,
            .y = 0.0f
        };
    }

    return (vec2) {
        .x = vector.x / length,
        .y = vector.y / length
    };
}
