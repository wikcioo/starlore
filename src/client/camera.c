#include "camera.h"

#include "config.h"
#include "window.h"

extern vec2 main_window_size;

void camera_create(camera_t *out_camera, vec2 position)
{
    out_camera->position = position;
    out_camera->zoom = 1.0f;
    camera_recalculate_projection(out_camera);
}

void camera_move(camera_t *camera, vec2 offset)
{
    camera->position = vec2_add(camera->position, offset);
    camera_recalculate_projection(camera);
}

void camera_zoom(camera_t *camera, f32 offset)
{
    camera->zoom += offset;
    if (camera->zoom > CAMERA_ZOOM_MAX) {
        camera->zoom = CAMERA_ZOOM_MAX;
    } else if (camera->zoom < CAMERA_ZOOM_MIN) {
        camera->zoom = CAMERA_ZOOM_MIN;
    }

    camera_recalculate_projection(camera);
}

void camera_set_position(camera_t *camera, vec2 position)
{
    camera->position = position;
    camera_recalculate_projection(camera);
}

void camera_recalculate_projection(camera_t *camera)
{
    f32 left   = -main_window_size.x / 2.0f;
    f32 right  =  main_window_size.x / 2.0f;
    f32 bottom = -main_window_size.y / 2.0f;
    f32 top    =  main_window_size.y / 2.0f;

    f32 zoom = camera->zoom;
    camera->projection = mat4_multiply(mat4_orthographic(left / zoom, right / zoom, bottom / zoom, top / zoom, -1.0f, 1.0f),
                                       mat4_translate(vec2_negate(camera->position)));
}
