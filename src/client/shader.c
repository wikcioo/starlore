#include "shader.h"

#include <stdio.h>
#include <stdlib.h>

#include <glad/glad.h>

#include "common/logger.h"
#include "common/asserts.h"
#include "common/filesystem.h"

b8 shader_create(const shader_create_info_t *create_info, shader_t *out_shader)
{
    ASSERT(create_info)
    ASSERT(out_shader);
    ASSERT(create_info->vertex_filepath);
    ASSERT(create_info->fragment_filepath);

    if (!filesystem_exists(create_info->vertex_filepath) ||
        !filesystem_exists(create_info->fragment_filepath)) {
        return false;
    }

    file_handle_t vertex_file_handle;
    file_handle_t fragment_file_handle;
    filesystem_open(create_info->vertex_filepath, FILE_MODE_READ, false, &vertex_file_handle);
    filesystem_open(create_info->fragment_filepath, FILE_MODE_READ, false, &fragment_file_handle);

    u64 vertex_file_size = 0, fragment_file_size = 0;
    filesystem_get_size(&vertex_file_handle, &vertex_file_size);
    filesystem_get_size(&fragment_file_handle, &fragment_file_size);

    // TODO: Replace with custom allocator
    char *vertex_source_buffer = (char *)malloc(vertex_file_size+1);
    char *fragment_source_buffer = (char *)malloc(fragment_file_size+1);

    if (!filesystem_read_all(&vertex_file_handle, vertex_source_buffer, NULL)) {
        LOG_ERROR("failed to read all bytes from file at '%s'", create_info->vertex_filepath);
    }
    if (!filesystem_read_all(&fragment_file_handle, fragment_source_buffer, NULL)) {
        LOG_ERROR("failed to read all bytes from file at '%s'", create_info->fragment_filepath);
    }

    vertex_source_buffer[vertex_file_size] = '\0';
    fragment_source_buffer[fragment_file_size] = '\0';

    filesystem_close(&vertex_file_handle);
    filesystem_close(&fragment_file_handle);

    u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, (const char **)&vertex_source_buffer, NULL);
    glCompileShader(vertex_shader);

    i32 is_compiled;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &is_compiled);
    if (is_compiled == GL_FALSE)
    {
        char info_log[1024];
        glGetShaderInfoLog(vertex_shader, 1024, 0, info_log);
        LOG_ERROR("failed to compile vertex shader: %s", info_log);
    }

    u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, (const char **)&fragment_source_buffer, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &is_compiled);
    if (is_compiled == GL_FALSE)
    {
        char info_log[1024];
        glGetShaderInfoLog(fragment_shader, 1024, 0, info_log);
        LOG_ERROR("failed to compile fragment shader: %s", info_log);
    }

    free(vertex_source_buffer);
    free(fragment_source_buffer);

    u32 program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    i32 is_linked;
    glGetProgramiv(program, GL_LINK_STATUS, &is_linked);
    if (!is_linked) {
        LOG_ERROR("failed to link the program");
        return false;
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    out_shader->program = program;

    return true;
}

void shader_destroy(shader_t *shader)
{
    ASSERT(shader);
    glDeleteProgram(shader->program);
}

void shader_bind(shader_t *shader)
{
    ASSERT(shader);
    glUseProgram(shader->program);
}

void shader_unbind(shader_t *shader)
{
    ASSERT(shader);
    glUseProgram(0);
}

void shader_set_uniform_int(shader_t *shader, const char *name, i32 data)
{
    ASSERT(shader);
    ASSERT(name);

    i32 location = glGetUniformLocation(shader->program, name);
    ASSERT(location != -1);
    glUniform1i(location, data);
}

void shader_set_uniform_vec2(shader_t *shader, const char *name, vec2 *data)
{
    ASSERT(shader);
    ASSERT(name);
    ASSERT(data);

    i32 location = glGetUniformLocation(shader->program, name);
    ASSERT(location != -1);
    glUniform2fv(location, 1, (f32 *)data);
}

void shader_set_uniform_vec3(shader_t *shader, const char *name, vec3 *data)
{
    ASSERT(shader);
    ASSERT(name);
    ASSERT(data);

    i32 location = glGetUniformLocation(shader->program, name);
    ASSERT(location != -1);
    glUniform3fv(location, 1, (f32 *)data);
}

void shader_set_uniform_vec4(shader_t *shader, const char *name, vec4 *data)
{
    ASSERT(shader);
    ASSERT(name);
    ASSERT(data);

    i32 location = glGetUniformLocation(shader->program, name);
    ASSERT(location != -1);
    glUniform4fv(location, 1, (f32 *)data);
}

void shader_set_uniform_mat4(shader_t *shader, const char *name, mat4 *data)
{
    ASSERT(shader);
    ASSERT(name);
    ASSERT(data);

    i32 location = glGetUniformLocation(shader->program, name);
    ASSERT(location != -1);
    glUniformMatrix4fv(location, 1, false, (f32 *)data);
}
