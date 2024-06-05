#include "perlin_noise.h"

#include <math.h>
#include <memory.h>
#include <stdlib.h>

#include "common/asserts.h"

#define PERLIN_SIZE 256

static i32 perm[PERLIN_SIZE * 2];
static f32 grad[PERLIN_SIZE * 2][2];

static f32 fade(f32 t)
{
    return t * t * t * (t * (t * 6 - 15) + 10);
}

static f32 lerp(f32 t, f32 a, f32 b)
{
    return a + t * (b - a);
}

static f32 grad_dot(i32 hash, f32 x, f32 y)
{
    i32 h = hash & 7;
    f32 u = h < 4 ? x : y;
    f32 v = h < 4 ? y : x;
    return ((h & 1) ? -u : u) + ((h & 2) ? -2.0 * v : 2.0 * v);
}

static f32 perlin(f32 x, f32 y)
{
    i32 X = (i32)floor(x) & (PERLIN_SIZE - 1);
    i32 Y = (i32)floor(y) & (PERLIN_SIZE - 1);
    x -= floor(x);
    y -= floor(y);
    f32 u = fade(x);
    f32 v = fade(y);
    i32 aa = perm[X] + Y;
    i32 ab = perm[X] + Y + 1;
    i32 ba = perm[X + 1] + Y;
    i32 bb = perm[X + 1] + Y + 1;
    return lerp(
        v,
        lerp(u, grad_dot(perm[aa], x, y    ), grad_dot(perm[ba], x - 1, y    )),
        lerp(u, grad_dot(perm[ab], x, y - 1), grad_dot(perm[bb], x - 1, y - 1))
    );
}

void init_perlin_noise(u32 seed)
{
    srand(seed);
    for (i32 i = 0; i < PERLIN_SIZE; i++) {
        perm[i] = i;
        grad[i][0] = (f32)(rand() % (PERLIN_SIZE * 2) - PERLIN_SIZE) / PERLIN_SIZE;
        grad[i][1] = (f32)(rand() % (PERLIN_SIZE * 2) - PERLIN_SIZE) / PERLIN_SIZE;
        f32 length = sqrt(grad[i][0] * grad[i][0] + grad[i][1] * grad[i][1]);
        grad[i][0] /= length;
        grad[i][1] /= length;
    }
    for (i32 i = PERLIN_SIZE - 1; i > 0; i--) {
        i32 j = rand() % (i + 1);
        i32 tmp = perm[i];
        perm[i] = perm[j];
        perm[j] = tmp;
    }
    for (i32 i = 0; i < PERLIN_SIZE; i++) {
        perm[PERLIN_SIZE + i] = perm[i];
        grad[PERLIN_SIZE + i][0] = grad[i][0];
        grad[PERLIN_SIZE + i][1] = grad[i][1];
    }
}

void perlin_noise_generate_2d(perlin_noise_config_t config, f32 *output)
{
    ASSERT(output);

    init_perlin_noise(config.seed);

    for (i32 x = 0; x < config.width; x++) {
        for (i32 y = 0; y < config.height; y++) {
            f32 noise = 0.0f;
            f32 scale_accumulator = 0.0f;
            f32 scale = 1.0f;

            for (i32 o = 0; o < config.octave_count; o++) {
                f32 pitch = config.width >> o;
                if (pitch == 0) {
                    pitch = 1;
                }

                f32 sample_x = (config.pos_x + x) / pitch;
                f32 sample_y = (config.pos_y + y) / pitch;

                noise += perlin(sample_x, sample_y) * scale;
                scale_accumulator += scale;
                scale /= config.scaling_bias;
            }

            noise /= scale_accumulator;
            noise = (noise + 1.0f) / 2.0f;
            if (noise < 0.0f) noise = 0.0f;
            if (noise > 1.0f) noise = 1.0f;
            output[y * config.width + x] = noise;
        }
    }
}
