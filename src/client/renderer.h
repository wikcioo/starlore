#pragma once

#include "defines.h"
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

u32 renderer_get_font_height(font_atlas_size_e fa);
u32 renderer_get_font_width(font_atlas_size_e fa);

void renderer_draw_text(const char *text, font_atlas_size_e fa_size, vec2 position, f32 scale, vec3 color, f32 alpha);
void renderer_draw_quad(vec2 position, vec2 size, f32 rotation_angle, vec3 color, f32 alpha);
