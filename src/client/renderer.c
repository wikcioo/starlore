#include "renderer.h"

#include <glad/glad.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "defines.h"
#include "shader.h"
#include "config.h"
#include "window.h"
#include "color_palette.h"
#include "common/logger.h"
#include "common/asserts.h"

#define MONOGRAM_FONT_FILEPATH "assets/fonts/monogram.ttf"

typedef struct {
    u32 vao;
    u32 vbo;
} font_data_t;

typedef struct {
    vec2 size;
    vec2 bearing;
    vec2 advance;
    f32 xoffset;
} glyph_data_t;

typedef struct {
    u32 texture;
    u32 width;
    u32 height;
    glyph_data_t glyphs[128];
} font_atlas_t;

typedef struct {
    u32 vao;
    u32 vbo;
    u32 ibo;
} quad_data_t;

static shader_t font_shader;
static shader_t quad_shader;

static font_data_t font_data;
static font_atlas_t font_atlases[FA_COUNT];

static texture_t white_texture;
static quad_data_t quad_data;

static FT_Library ft;
static FT_Face face;

static b8 default_quad_vertices_in_buffer = true;
static vertex_2d default_vertices[4] = {
    { { .x=-0.5f, .y=-0.5f }, { .u=0.0f, .v=0.0f } },
    { { .x=-0.5f, .y= 0.5f }, { .u=0.0f, .v=1.0f } },
    { { .x= 0.5f, .y= 0.5f }, { .u=1.0f, .v=1.0f } },
    { { .x= 0.5f, .y=-0.5f }, { .u=1.0f, .v=0.0f } }
};

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
    glGenerateMipmap(GL_TEXTURE_2D);

    float borderColor[] = {1.0f, 0.0f, 0.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

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

static void create_font_data(void)
{
    glGenVertexArrays(1, &font_data.vao);
    glBindVertexArray(font_data.vao);

    glGenBuffers(1, &font_data.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, font_data.vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_2d), 0);

    if (FT_Init_FreeType(&ft)) {
        LOG_FATAL("failed to initialize freetype library");
    }

    if (FT_New_Face(ft, MONOGRAM_FONT_FILEPATH, 0, &face)) {
        LOG_ERROR("failed to load font at %s", MONOGRAM_FONT_FILEPATH);
    }

    create_font_atlas(face, 16, &font_atlases[FA16]);
    create_font_atlas(face, 32, &font_atlases[FA32]);
    create_font_atlas(face, 64, &font_atlases[FA64]);
    create_font_atlas(face, 128, &font_atlases[FA128]);
}

static void create_quad_data(void)
{
    u32 indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    glGenVertexArrays(1, &quad_data.vao);
    glBindVertexArray(quad_data.vao);

    glGenBuffers(1, &quad_data.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, quad_data.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(default_vertices), (void *)default_vertices, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &quad_data.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quad_data.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), (void *)indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), 0);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void *)(2 * sizeof(f32)));

    texture_specification_t spec = {
        .width = 1,
        .height = 1,
        .format = IMAGE_FORMAT_RGBA8,
        .generate_mipmaps = false
    };
    u32 white_image_data = 0xFFFFFFFF;
    texture_create_from_spec(spec, &white_image_data, &white_texture);
}

static void create_shaders(void)
{
    shader_create_info_t font_shader_create_info = {
        .vertex_filepath = "assets/shaders/text.vert",
        .fragment_filepath = "assets/shaders/text.frag"
    };

    if (!shader_create(&font_shader_create_info, &font_shader)) {
        LOG_ERROR("failed to create font shader");
    }

    shader_create_info_t quad_shader_create_info = {
        "assets/shaders/quad.vert",
        "assets/shaders/quad.frag"
    };

    if (!shader_create(&quad_shader_create_info, &quad_shader)) {
        LOG_ERROR("failed to create quad shader");
    }
}

b8 renderer_init(void)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    create_font_data();
    create_quad_data();
    create_shaders();

    return true;
}

void renderer_shutdown(void)
{
    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    shader_destroy(&font_shader);
    shader_destroy(&quad_shader);

    glDeleteVertexArrays(1, &font_data.vao);
    glDeleteVertexArrays(1, &quad_data.vao);
    glDeleteBuffers(1, &font_data.vbo);
    glDeleteBuffers(1, &quad_data.vbo);
    glDeleteBuffers(1, &quad_data.ibo);
}

void renderer_clear_screen(vec4 color)
{
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

u32 renderer_get_font_height(font_atlas_size_e fa)
{
    return font_atlases[fa].height;
}

u32 renderer_get_font_width(font_atlas_size_e fa)
{
    // NOTE: Works only for monospaced fonts
    return font_atlases[fa].glyphs[32].advance.x;
}

void renderer_begin_scene(camera_t *camera)
{
    shader_bind(&font_shader);
    shader_set_uniform_mat4(&font_shader, "u_projection", &camera->projection);

    shader_bind(&quad_shader);
    shader_set_uniform_mat4(&quad_shader, "u_projection", &camera->projection);
}

void renderer_draw_text(const char *text, font_atlas_size_e fa_size, vec2 position, f32 scale, vec3 color, f32 alpha)
{
    ASSERT(text);
    ASSERT(fa_size < FA_COUNT);

    vec4 font_color = vec4_create_from_vec3(color, alpha);
    shader_bind(&font_shader);
    shader_set_uniform_int(&font_shader, "u_texture", 0);
    shader_set_uniform_vec4(&font_shader, "u_color", &font_color);

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

void renderer_draw_quad(vec2 position, vec2 size, f32 rotation_angle, vec3 color, f32 alpha)
{
    shader_bind(&quad_shader);
    vec4 quad_color = vec4_create_from_vec3(color, alpha);
    shader_set_uniform_int(&quad_shader, "u_texture", 0);
    shader_set_uniform_vec4(&quad_shader, "u_color", &quad_color);

    mat4 translation = mat4_translate(position);
    mat4 rotation = mat4_rotate(rotation_angle);
    mat4 scale = mat4_scale(size);
    mat4 model = mat4_multiply(translation, mat4_multiply(rotation, scale));
    shader_set_uniform_mat4(&quad_shader, "u_model", &model);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, white_texture.id);
    glBindVertexArray(quad_data.vao);
    if (!default_quad_vertices_in_buffer) {
        glBindBuffer(GL_ARRAY_BUFFER, quad_data.vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_2d) * 4, default_vertices, GL_DYNAMIC_DRAW);
        default_quad_vertices_in_buffer = true;
    }
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
}

void renderer_draw_sprite(texture_t *texture, vec2 position, f32 scale, f32 rotation_angle)
{
    renderer_draw_sprite_color(texture, position, scale, rotation_angle, vec3_create(1.0f, 1.0f, 1.0f), 1.0f);
}

void renderer_draw_sprite_color(texture_t *texture, vec2 position, f32 scale, f32 rotation_angle, vec3 color, f32 alpha)
{
    shader_bind(&quad_shader);
    vec4 quad_color = vec4_create_from_vec3(color, alpha);
    shader_set_uniform_int(&quad_shader, "u_texture", 0);
    shader_set_uniform_vec4(&quad_shader, "u_color", &quad_color);

    mat4 translation_matrix = mat4_translate(position);
    mat4 rotation_matrix = mat4_rotate(rotation_angle);
    mat4 scale_matrix = mat4_scale(vec2_create(texture->width * scale, texture->height * scale));
    mat4 model_matrix = mat4_multiply(translation_matrix, mat4_multiply(rotation_matrix, scale_matrix));
    shader_set_uniform_mat4(&quad_shader, "u_model", &model_matrix);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture->id);
    glBindVertexArray(quad_data.vao);
    if (!default_quad_vertices_in_buffer) {
        glBindBuffer(GL_ARRAY_BUFFER, quad_data.vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_2d) * 4, default_vertices, GL_DYNAMIC_DRAW);
        default_quad_vertices_in_buffer = true;
    }
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
}

void renderer_draw_sprite_uv_color(texture_t *texture, vec2 uv[4], vec2 size, vec2 position, f32 scale, f32 rotation_angle, vec3 color, f32 alpha)
{
    shader_bind(&quad_shader);
    shader_set_uniform_int(&quad_shader, "u_texture", 0);
    vec4 sprite_color = vec4_create_from_vec3(color, alpha);
    shader_set_uniform_vec4(&quad_shader, "u_color", &sprite_color);

    mat4 translation_matrix = mat4_translate(position);
    mat4 rotation_matrix = mat4_rotate(rotation_angle);
    mat4 scale_matrix = mat4_scale(vec2_create(size.x * scale, size.y * scale));
    mat4 model_matrix = mat4_multiply(translation_matrix, mat4_multiply(rotation_matrix, scale_matrix));
    shader_set_uniform_mat4(&quad_shader, "u_model", &model_matrix);

    vertex_2d vertices[4] = {0};
    memcpy(vertices, default_vertices, sizeof(vertex_2d) * 4);
    memcpy(&vertices[0].tex_coord, &uv[0], sizeof(vec2));
    memcpy(&vertices[1].tex_coord, &uv[1], sizeof(vec2));
    memcpy(&vertices[2].tex_coord, &uv[2], sizeof(vec2));
    memcpy(&vertices[3].tex_coord, &uv[3], sizeof(vec2));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture->id);
    glBindVertexArray(quad_data.vao);
    glBindBuffer(GL_ARRAY_BUFFER, quad_data.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_2d) * 4, vertices, GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
    default_quad_vertices_in_buffer = false;
}
