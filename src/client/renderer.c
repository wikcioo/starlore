#include "renderer.h"

#include <glad/glad.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "defines.h"
#include "shader.h"
#include "color_palette.h"
#include "common/logger.h"
#include "common/asserts.h"

static const char *monogram_font_filepath = "assets/fonts/monogram.ttf";

extern mat4 ortho_projection;

typedef struct font_data {
    shader_t shader;
    u32 vao;
    u32 vbo;
} font_data_t;

typedef struct glyph_data {
    vec2 size;
    vec2 bearing;
    vec2 advance;
    f32 xoffset;
} glyph_data_t;

typedef struct font_atlas {
    u32 texture;
    u32 width;
    u32 height;
    glyph_data_t glyphs[128];
} font_atlas_t;

static font_data_t font_data;
static font_atlas_t font_atlases[FA_COUNT];

static FT_Library ft;
static FT_Face face;

static void create_font_atlas(FT_Face ft_face, u32 height, font_atlas_t *out_atlas)
{
    ASSERT(out_atlas);

    if (FT_Set_Pixel_Sizes(ft_face, 0, height) != 0) {
        LOG_ERROR("failed to set new pixel size");
        return;
    }

    memset(out_atlas, 0, sizeof(font_atlas_t));

    i32 w = 0, h = 0;
    FT_GlyphSlot g = ft_face->glyph;
    for (i32 i = 32; i < 128; i++) {
        if (FT_Load_Char(ft_face, i, FT_LOAD_RENDER) != 0) {
            LOG_ERROR("failed to character '%c'", i);
            continue;
        }

        w += g->bitmap.width;
        h = math_max(h, g->bitmap.rows);
    }

    out_atlas->width = w;
    out_atlas->height = h;

    // Glyphs are in a 1-byte greyscale format, so disable the default 4-byte alignment restrictions
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &out_atlas->texture);
    glBindTexture(GL_TEXTURE_2D, out_atlas->texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, 0);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    i32 x = 0;
    for (i32 i = 32; i < 128; i++) {
        if (FT_Load_Char(ft_face, i, FT_LOAD_RENDER)) {
            continue;
        }

        out_atlas->glyphs[i].size    = vec2_create(g->bitmap.width, g->bitmap.rows);
        out_atlas->glyphs[i].bearing = vec2_create(g->bitmap_left, g->bitmap_top);
        out_atlas->glyphs[i].advance = vec2_create(g->advance.x >> 6, g->advance.y >> 6);
        out_atlas->glyphs[i].xoffset = (f32)x / w;

        glTexSubImage2D(GL_TEXTURE_2D, 0, x, 0, g->bitmap.width, g->bitmap.rows, GL_RED, GL_UNSIGNED_BYTE, g->bitmap.buffer);
        x += g->bitmap.width;
    }
}

static b8 create_font_data(void)
{
    shader_create_info_t create_info = {
        .vertex_filepath = "assets/shaders/text.vert",
        .fragment_filepath = "assets/shaders/text.frag"
    };

    if (!shader_create(&create_info, &font_data.shader)) {
        LOG_ERROR("failed to create text shader");
        return false;
    }

    shader_bind(&font_data.shader);
    shader_set_uniform_int(&font_data.shader, "u_texture", 0);

    glGenVertexArrays(1, &font_data.vao);
    glBindVertexArray(font_data.vao);

    glGenBuffers(1, &font_data.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, font_data.vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_2d), 0);

    if (FT_Init_FreeType(&ft)) {
        LOG_FATAL("failed to initialize freetype library");
        return false;
    }

    if (FT_New_Face(ft, monogram_font_filepath, 0, &face)) {
        LOG_ERROR("failed to load font at %s", monogram_font_filepath);
        return false;
    }

    create_font_atlas(face, 16, &font_atlases[FA16]);
    create_font_atlas(face, 32, &font_atlases[FA32]);
    create_font_atlas(face, 64, &font_atlases[FA64]);
    create_font_atlas(face, 128, &font_atlases[FA128]);

    return true;
}

b8 renderer_init(void)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (!create_font_data()) {
        return false;
    }

    return true;
}

void renderer_shutdown(void)
{
    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

u32 renderer_get_font_height(font_atlas_size_e fa)
{
    return font_atlases[fa].height;
}

void renderer_draw_text(const char *text, font_atlas_size_e fa_size, vec2 position, f32 scale, vec3 color, f32 alpha)
{
    ASSERT(text);
    ASSERT(fa_size < FA_COUNT);

    vec4 font_color = vec4_create_from_vec3(color, alpha);
    shader_bind(&font_data.shader);
    shader_set_uniform_vec4(&font_data.shader, "u_color", &font_color);
    shader_set_uniform_mat4(&font_data.shader, "u_projection", &ortho_projection);

    i32 n = 0;
    f32 start_x = position.x;
    vertex_2d coords[6 * strlen(text)];
    memset(coords, 0, 6 * strlen(text));

    font_atlas_t *fa = &font_atlases[fa_size];
    for (const char *c = text; *c; c++) {
        glyph_data_t g = fa->glyphs[(i32)*c];

        if (*c == ' ') {
            position.x += g.advance.x * scale;
        } else if (*c == '\n') {
            position.x = start_x;
            position.y -= fa->height * scale;
        } else if (*c == '\t') {
            // Tab is 4 spaces
            position.x += 4 * fa->glyphs[32].advance.x * scale;
        } else {
            f32 x =  position.x + g.bearing.x * scale;
            f32 y = -position.y - g.bearing.y * scale;
            f32 w = g.size.x * scale;
            f32 h = g.size.y * scale;

            if (!w || !h) {
                continue;
            }

            f32 ox = g.xoffset;
            f32 sx = g.size.x;
            f32 sy = g.size.y;

            coords[n++] = (vertex_2d){ { .x=  x, .y=  -y }, { .u=             ox, .v=            0 } };
            coords[n++] = (vertex_2d){ { .x=x+w, .y=  -y }, { .u=ox+sx/fa->width, .v=            0 } };
            coords[n++] = (vertex_2d){ { .x=  x, .y=-y-h }, { .u=             ox, .v=sy/fa->height } };

            coords[n++] = (vertex_2d){ { .x=x+w, .y=  -y }, { .u=ox+sx/fa->width, .v=            0 } };
            coords[n++] = (vertex_2d){ { .x=  x, .y=-y-h }, { .u=             ox, .v=sy/fa->height } };
            coords[n++] = (vertex_2d){ { .x=x+w, .y=-y-h }, { .u=ox+sx/fa->width, .v=sy/fa->height } };

            position.x += g.advance.x * scale;
            position.y += g.advance.y * scale;
        }
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fa->texture);
    glBindVertexArray(font_data.vao);
    glBindBuffer(GL_ARRAY_BUFFER, font_data.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(coords), coords, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, n);
}
