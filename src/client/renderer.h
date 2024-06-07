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

typedef struct {
    u32 quad_count;
    u32 char_count;
    u32 draw_calls;
} renderer_stats_t;

extern renderer_stats_t renderer_stats;

b8 renderer_init(void);
void renderer_shutdown(void);

void renderer_begin_scene(camera_t *camera);
void renderer_end_scene(void);

void renderer_reset_stats(void);
void renderer_clear_screen(vec4 color);

void renderer_draw_quad_color(vec2 position, vec2 size, f32 rotation_angle, vec3 color, f32 alpha);
void renderer_draw_quad_sprite(vec2 position, vec2 size, f32 rotation_angle, texture_t *texture);
void renderer_draw_quad_sprite_uv(vec2 position, vec2 size, f32 rotation_angle, texture_t *texture, const vec2 uv[4]);
void renderer_draw_quad_sprite_color(vec2 position, vec2 size, f32 rotation_angle, vec3 color, f32 alpha, texture_t *texture);
void renderer_draw_quad_sprite_color_uv(vec2 position, vec2 size, f32 rotation_angle, vec3 color, f32 alpha, texture_t *texture, const vec2 uv[4]);

void renderer_draw_rect(vec2 position, vec2 size, vec3 color, f32 alpha);

void renderer_draw_circle(vec2 position, f32 radius, vec3 color, f32 alpha);
void renderer_draw_circle_thick(vec2 position, f32 radius, f32 thickness, vec3 color, f32 alpha);
void renderer_draw_circle_thick_and_fade(vec2 position, f32 radius, f32 thickness, f32 fade, vec3 color, f32 alpha);

void renderer_draw_line(vec2 p1, vec2 p2, vec3 color, f32 alpha);
void renderer_set_line_width(f32 width);

void renderer_draw_text(const char *text, font_atlas_size_e fa_size, vec2 position, f32 scale, vec3 color, f32 alpha);

u32 renderer_get_font_bearing_y(font_atlas_size_e fa);
u32 renderer_get_font_height(font_atlas_size_e fa);
u32 renderer_get_font_width(font_atlas_size_e fa);
