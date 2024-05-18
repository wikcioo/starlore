#include "camera.h"

#include "window.h"

void camera_create(camera_t *out_camera, vec2 position)
{
    vec2 window_size = window_get_size();
    out_camera->position = position;
    out_camera->projection = mat4_orthographic(0.0f, window_size.x, 0.0f, window_size.y, -1.0f, 1.0f);
}

void camera_move(camera_t *camera, vec2 offset)
{
    vec2 window_size = window_get_size();
    camera->position = vec2_add(camera->position, offset);
    camera->projection = mat4_multiply(mat4_orthographic(0.0f, window_size.x, 0.0f, window_size.y, -1.0f, 1.0f),
                                       mat4_translate(vec2_negate(camera->position)));
}

void camera_set_position(camera_t *camera, vec2 position)
{
    vec2 window_size = window_get_size();
    camera->position = position;
    camera->projection = mat4_multiply(mat4_orthographic(0.0f, window_size.x, 0.0f, window_size.y, -1.0f, 1.0f),
                                       mat4_translate(vec2_negate(camera->position)));
}

void camera_resize(camera_t *camera, u32 width, u32 height)
{
    camera->projection = mat4_multiply(mat4_orthographic(0.0f, width, 0.0f, height, -1.0f, 1.0f),
                                       mat4_translate(vec2_negate(camera->position)));
}
