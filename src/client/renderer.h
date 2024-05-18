#pragma once

#include "defines.h"
#include "event.h"
#include "camera.h"
#include "texture.h"
#include "common/maths.h"

typedef enum {
    FA16,
    FA32,
    FA64,
    FA128,
    FA_COUNT
} font_atlas_size_e;

b8 renderer_init(void);
void renderer_shutdown(void);

void renderer_clear_screen(vec4 color);

u32 renderer_get_font_height(font_atlas_size_e fa);
u32 renderer_get_font_width(font_atlas_size_e fa);

void renderer_begin_scene(camera_t *camera);

void renderer_draw_text(const char *text, font_atlas_size_e fa_size, vec2 position, f32 scale, vec3 color, f32 alpha);
void renderer_draw_quad(vec2 position, vec2 size, f32 rotation_angle, vec3 color, f32 alpha);
void renderer_draw_sprite(texture_t *texture, vec2 position, f32 scale, f32 rotation_angle);
void renderer_draw_sprite_color(texture_t *texture, vec2 position, f32 scale, f32 rotation_angle, vec3 color, f32 alpha);
void renderer_draw_sprite_uv_color(texture_t *texture, vec2 uv[4], vec2 size, vec2 position, f32 scale, f32 rotation_angle, vec3 color, f32 alpha);
