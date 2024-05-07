#pragma once

#include "defines.h"

#define MATHS_PI 3.1415926f

#define DEG_TO_RAD(deg) (deg * MATHS_PI / 180.0f)
#define RAD_TO_DEF(rad) (rad * 180.0f / MATHS_PI)

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

// NOTE: all matrix functions assume column-major order
typedef struct mat4 {
    f32 data[16];
} mat4;

f32 maths_sinf(f32 value);
f32 maths_cosf(f32 value);

/* returns value between 0 (inclusive) and RAND_MAX(INT_MAX) (exclusive) */
i32 maths_random(void);
/* min is inclusive, max is exclusive */
i32 maths_random_range(i32 min, i32 max);

/* returns value between 0.0 (inclusive) and 1.0 (exclusive) */
f32 maths_frandom(void);
/* min is inclusive, max is exclusive */
f32 maths_frandom_range(f32 min, f32 max);

INLINE f32 maths_lerpf(f32 value_0, f32 value_1, f32 t) {
    return value_0 + t * (value_1 - value_0);
}

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

INLINE mat4 mat4_identity(void)
{
    mat4 matrix = {0};

    matrix.data[0] = 1.0f;
    matrix.data[5] = 1.0f;
    matrix.data[10] = 1.0f;
    matrix.data[15] = 1.0f;

    return matrix;
}

// NOTE: multiplication order is matrix_0 * matrix_1
INLINE mat4 mat4_multiply(mat4 matrix_0, mat4 matrix_1)
{
    mat4 matrix = mat4_identity();

    const f32* m1_ptr = matrix_1.data;
    const f32* m2_ptr = matrix_0.data;
    f32* dst_ptr = matrix.data;

    for (i32 i = 0; i < 4; i++) {
        for (i32 j = 0; j < 4; j++) {
            *dst_ptr = m1_ptr[0] * m2_ptr[0 + j] +
                       m1_ptr[1] * m2_ptr[4 + j] +
                       m1_ptr[2] * m2_ptr[8 + j] +
                       m1_ptr[3] * m2_ptr[12 + j];
            dst_ptr++;
        }
        m1_ptr += 4;
    }

    return matrix;
}

INLINE mat4 mat4_orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far)
{
    mat4 matrix = mat4_identity();

    f32 rl = 1.0f / (right - left);
    f32 tb = 1.0f / (top - bottom);
    f32 fn = 1.0f / (far - near);

    matrix.data[0] = 2.0f * rl;
    matrix.data[5] = 2.0f * tb;
    matrix.data[10] = -2.0f * fn;
    matrix.data[12] = -(right + left) * rl;
    matrix.data[13] = -(top + bottom) * tb;
    matrix.data[14] = -(far + near) * fn;

    return matrix;
}

INLINE mat4 mat4_translate(vec2 position)
{
    mat4 matrix = mat4_identity();

    matrix.data[12] = position.x;
    matrix.data[13] = position.y;

    return matrix;
}

// NOTE: angle_deg produces clockwise rotation for positive values
INLINE mat4 mat4_rotate(f32 angle_deg)
{
    mat4 matrix = mat4_identity();

    f32 c = maths_cosf(DEG_TO_RAD(angle_deg));
    f32 s = maths_sinf(DEG_TO_RAD(angle_deg));

    matrix.data[0] = c;
    matrix.data[1] = -s;
    matrix.data[4] = s;
    matrix.data[5] = c;

    return matrix;
}

INLINE mat4 mat4_scale(vec2 scale)
{
    mat4 matrix = mat4_identity();

    matrix.data[0] = scale.x;
    matrix.data[5] = scale.y;

    return matrix;
}
