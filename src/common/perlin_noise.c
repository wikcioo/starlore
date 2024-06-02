#include "perlin_noise.h"

#include <memory.h>
#include <stdlib.h>

#include "common/asserts.h"

void perlin_noise_generate_2d(perlin_noise_config_t config, f32 *output)
{
    ASSERT(config.width > 0 && config.height > 0);
    ASSERT(output);

    u32 output_length = config.width * config.height;
    u32 output_size = output_length * sizeof(f32);

    memset(output, 0, output_size);
    srand(config.seed);
    f32 *noise_array = malloc(output_size);
    for (u32 i = 0; i < output_length; i++) {
        noise_array[i] = (f32)rand() / (f32)RAND_MAX;
    }

    for (i32 x = 0; x < config.width; x++) {
        for (i32 y = 0; y < config.height; y++) {
            f32 noise = 0.0f;
            f32 scale_accumulator = 0.0f;
            f32 scale = 1.0f;

            for (i32 o = 0; o < config.octave_count; o++) {
                i32 pitch = config.width >> o;
                i32 sample_x1 = (x / pitch) * pitch;
                i32 sample_y1 = (y / pitch) * pitch;
                i32 sample_x2 = (sample_x1 + pitch) % config.width;
                i32 sample_y2 = (sample_y1 + pitch) % config.width;

                f32 blend_x = (f32)(x - sample_x1) / (f32)pitch;
                f32 blend_y = (f32)(y - sample_y1) / (f32)pitch;

                f32 sample_t = (1.0f - blend_x) * noise_array[sample_y1 * config.width + sample_x1] + blend_x * noise_array[sample_y1 * config.width + sample_x2];
                f32 sample_b = (1.0f - blend_x) * noise_array[sample_y2 * config.width + sample_x1] + blend_x * noise_array[sample_y2 * config.width + sample_x2];

                scale_accumulator += scale;
                noise += (blend_y * (sample_b - sample_t) + sample_t) * scale;
                scale = scale / config.scaling_bias;
            }

            output[y * config.width + x] = noise / scale_accumulator;
        }
    }

    free(noise_array);
}
