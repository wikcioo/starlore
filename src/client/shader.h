#pragma once

#include "defines.h"

typedef struct shader
{
    u32 program;
} shader_t;

typedef struct shader_create_info
{
    const char *vertex_filepath;
    const char *fragment_filepath;
} shader_create_info_t;

b8 shader_create(const shader_create_info_t *create_info, shader_t *out_shader);
void shader_destroy(shader_t *shader);

void shader_bind(shader_t *shader);
void shader_unbind(shader_t *shader);

void shader_set_uniform_vec2(shader_t *shader, const char *name, f32 x, f32 y);
void shader_set_uniform_vec3(shader_t *shader, const char *name, f32 x, f32 y, f32 z);
