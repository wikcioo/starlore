#pragma once

#include "defines.h"

typedef struct {
    u32 width;
    u32 height;
    u32 seed;
    i32 octave_count;
    f32 scaling_bias;
} perlin_noise_config_t;

void perlin_noise_generate_2d(perlin_noise_config_t config, f32 *output);
