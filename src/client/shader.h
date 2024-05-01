#pragma once

#include "defines.h"
#include "common/maths.h"

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

void shader_set_uniform_vec2(shader_t *shader, const char *name, vec2 *data);
void shader_set_uniform_vec3(shader_t *shader, const char *name, vec3 *data);
void shader_set_uniform_mat4(shader_t *shader, const char *name, mat4 *data);
