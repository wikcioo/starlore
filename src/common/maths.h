#pragma once

#include "defines.h"

typedef union vec2 {
    f32 elements[2];
    struct {
        union {
            f32 x, r;
        };
        union {
            f32 y, g;
        };
    };
} vec2;

typedef union vec3 {
    f32 elements[3];
    struct {
        union {
            f32 x, r;
        };
        union {
            f32 y, g;
        };
        union {
            f32 z, b;
        };
    };
} vec3;

/* returns value between 0 (inclusive) and RAND_MAX(INT_MAX) (exclusive) */
i32 maths_random(void);
/* min is inclusive, max is exclusive */
i32 maths_random_range(i32 min, i32 max);

/* returns value between 0.0 (inclusive) and 1.0 (exclusive) */
f32 maths_frandom(void);
/* min is inclusive, max is exclusive */
f32 maths_frandom_range(f32 min, f32 max);

INLINE vec2 vec2_zero(void)
{
    return (vec2) {
        .x = 0.0f,
        .y = 0.0f
    };
}

INLINE vec2 vec2_create(f32 x, f32 y)
{
    return (vec2) {
        .x = x,
        .y = y
    };
}

INLINE vec2 vec2_add(vec2 v_0, vec2 v_1)
{
    return (vec2) {
        .x = v_0.x + v_1.x,
        .y = v_0.y + v_1.y
    };
}

INLINE vec2 vec2_sub(vec2 from, vec2 what)
{
    return (vec2) {
        .x = from.x - what.x,
        .y = from.y - what.y
    };
}

INLINE vec3 vec3_create(f32 x, f32 y, f32 z)
{
    return (vec3) {
        .x = x,
        .y = y,
        .z = z
    };
}
