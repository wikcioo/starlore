#pragma once

#include "event.h"
#include "common/maths.h"

typedef struct {
    vec2 position;
    mat4 projection;
} camera_t;

void camera_create(camera_t *out_camera, vec2 position);
void camera_move(camera_t *camera, vec2 offset);
void camera_set_position(camera_t *camera, vec2 position);
void camera_recalculate_projection(camera_t *camera);
