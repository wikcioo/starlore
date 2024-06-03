#include "renderer.h"

#include <glad/glad.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "defines.h"
#include "shader.h"
#include "config.h"
#include "window.h"
#include "common/logger.h"
#include "common/asserts.h"

#define FONT_FILEPATH "assets/fonts/monogram.ttf"

#define RENDERER_MAX_QUAD_COUNT     10000
#define RENDERER_MAX_VERTEX_COUNT   (RENDERER_MAX_QUAD_COUNT * 4)
#define RENDERER_MAX_INDEX_COUNT    (RENDERER_MAX_QUAD_COUNT * 6)
#define RENDERER_MAX_TEXTURE_COUNT  32

#define QUAD_VERTEX_COUNT 4

static FT_Library ft;
static FT_Face face;

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
    vec4 position;
    vec4 color;
    vec2 tex_coords;
    f32 tex_index;
} vertex_t;

typedef struct {
    // quad data
    u32 quad_va;
    u32 quad_vb;
    u32 quad_ib;
    u32 index_count;
    u32 white_texture_slot;
    texture_t white_texture;
    vertex_t *quad_buffer;
    vertex_t *quad_buffer_ptr;
    vec4 default_quad_vertex_positions[4];
    vec2 default_quad_vertex_tex_coords[4];
    u32 texture_slots[RENDERER_MAX_TEXTURE_COUNT];
    u32 texture_slot_index;
    shader_t quad_shader;
    // font data
    u32 font_va;
    u32 font_vb;
    font_atlas_t font_atlases[FA_COUNT];
    shader_t font_shader;
} renderer_data_t;

renderer_stats_t renderer_stats;
static renderer_data_t renderer_data;

static void start_batch(void);
static void next_batch(void);
static void flush(void);

static void create_shaders(void);
static void create_font_data(void);
static void create_font_atlas(FT_Face ft_face, u32 height, font_atlas_t *out_atlas);

b8 renderer_init(void)
{
    renderer_data.quad_buffer = malloc(RENDERER_MAX_VERTEX_COUNT * sizeof(vertex_t));
    memset(renderer_data.quad_buffer, 0, RENDERER_MAX_VERTEX_COUNT * sizeof(vertex_t));

    renderer_data.white_texture_slot = 0;
    renderer_data.texture_slot_index = 1;

    glCreateVertexArrays(1, &renderer_data.quad_va);
    glBindVertexArray(renderer_data.quad_va);

    glCreateBuffers(1, &renderer_data.quad_vb);
    glBindBuffer(GL_ARRAY_BUFFER, renderer_data.quad_vb);
    glBufferData(GL_ARRAY_BUFFER, RENDERER_MAX_VERTEX_COUNT * sizeof(vertex_t), NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (const void *)offsetof(vertex_t, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (const void *)offsetof(vertex_t, color));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (const void *)offsetof(vertex_t, tex_coords));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(vertex_t), (const void *)offsetof(vertex_t, tex_index));

    u32 indices[RENDERER_MAX_INDEX_COUNT];
    u32 offset = 0;
    for (u32 i = 0; i < RENDERER_MAX_INDEX_COUNT; i += 6) {
        indices[i + 0] = offset + 0;
        indices[i + 1] = offset + 1;
        indices[i + 2] = offset + 2;

        indices[i + 3] = offset + 2;
        indices[i + 4] = offset + 3;
        indices[i + 5] = offset + 0;

        offset += 4;
    }

    glCreateBuffers(1, &renderer_data.quad_ib);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, renderer_data.quad_ib);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    renderer_data.default_quad_vertex_positions[0] = vec4_create(-0.5f, -0.5f, 0.0f, 1.0f);
    renderer_data.default_quad_vertex_positions[1] = vec4_create(-0.5f,  0.5f, 0.0f, 1.0f);
    renderer_data.default_quad_vertex_positions[2] = vec4_create( 0.5f,  0.5f, 0.0f, 1.0f);
    renderer_data.default_quad_vertex_positions[3] = vec4_create( 0.5f, -0.5f, 0.0f, 1.0f);

    renderer_data.default_quad_vertex_tex_coords[0] = vec2_create(0.0f, 0.0f);
    renderer_data.default_quad_vertex_tex_coords[1] = vec2_create(0.0f, 1.0f);
    renderer_data.default_quad_vertex_tex_coords[2] = vec2_create(1.0f, 1.0f);
    renderer_data.default_quad_vertex_tex_coords[3] = vec2_create(1.0f, 0.0f);

    texture_specification_t spec = {
        .width = 1,
        .height = 1,
        .format = IMAGE_FORMAT_RGBA8,
        .generate_mipmaps = false
    };

    u32 white_texture_data = 0xFFFFFFFF;
    texture_create_from_spec(spec, &white_texture_data, &renderer_data.white_texture, "white");

    renderer_data.texture_slots[0] = renderer_data.white_texture.id;
    memset(&renderer_data.texture_slots[1], 0, RENDERER_MAX_TEXTURE_COUNT - 1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    create_shaders();

    i32 samplers[RENDERER_MAX_TEXTURE_COUNT];
    for (u32 i = 0; i < RENDERER_MAX_TEXTURE_COUNT; i++) {
        samplers[i] = i;
    }

    shader_bind(&renderer_data.quad_shader);
    shader_set_uniform_int_array(&renderer_data.quad_shader, "u_textures", samplers, 32);

    create_font_data();

    return true;
}

void renderer_shutdown(void)
{
    free(renderer_data.quad_buffer);

    glDeleteVertexArrays(1, &renderer_data.quad_va);
    glDeleteBuffers(1, &renderer_data.quad_vb);
    glDeleteBuffers(1, &renderer_data.quad_ib);

    texture_destroy(&renderer_data.white_texture);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    glDeleteVertexArrays(1, &renderer_data.font_va);
    glDeleteBuffers(1, &renderer_data.font_vb);

    shader_destroy(&renderer_data.font_shader);
    shader_destroy(&renderer_data.quad_shader);
}

void renderer_begin_scene(camera_t *camera)
{
    shader_bind(&renderer_data.font_shader);
    shader_set_uniform_mat4(&renderer_data.font_shader, "u_projection", &camera->projection);

    shader_bind(&renderer_data.quad_shader);
    shader_set_uniform_mat4(&renderer_data.quad_shader, "u_projection", &camera->projection);

    start_batch();
}

void renderer_end_scene(void)
{
    flush();
}

void renderer_reset_stats(void)
{
    memset(&renderer_stats, 0, sizeof(renderer_stats_t));
}

void renderer_clear_screen(vec4 color)
{
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void renderer_draw_quad_color(vec2 position, vec2 size, f32 rotation_angle, vec3 color, f32 alpha)
{
    if (renderer_data.index_count >= RENDERER_MAX_INDEX_COUNT) {
        next_batch();
    }

    f32 texture_index = 0.0f;

    mat4 translation_matrix = mat4_translate(position);
    mat4 rotation_matrix = mat4_rotate(rotation_angle);
    mat4 scale_matrix = mat4_scale(size);
    mat4 model_matrix = mat4_multiply(translation_matrix, mat4_multiply(rotation_matrix, scale_matrix));

    for (i32 i = 0; i < QUAD_VERTEX_COUNT; i++) {
        renderer_data.quad_buffer_ptr->position = mat4_multiply_vec4(model_matrix, renderer_data.default_quad_vertex_positions[i]);
        renderer_data.quad_buffer_ptr->color = vec4_create(color.r, color.g, color.b, alpha);
        renderer_data.quad_buffer_ptr->tex_coords = renderer_data.default_quad_vertex_tex_coords[i];
        renderer_data.quad_buffer_ptr->tex_index = texture_index;
        renderer_data.quad_buffer_ptr++;
    }

    renderer_data.index_count += 6;
    renderer_stats.quad_count++;
}

void renderer_draw_quad_sprite(vec2 position, vec2 size, f32 rotation_angle, texture_t *texture)
{
    renderer_draw_quad_sprite_uv(position, size, rotation_angle, texture, renderer_data.default_quad_vertex_tex_coords);
}

void renderer_draw_quad_sprite_uv(vec2 position, vec2 size, f32 rotation_angle, texture_t *texture, const vec2 uv[4])
{
    static const vec3 color = {{ 1.0f, 1.0f, 1.0f }};
    static const f32 alpha = 1.0f;

    renderer_draw_quad_sprite_color_uv(position, size, rotation_angle, color, alpha, texture, uv);
}

void renderer_draw_quad_sprite_color(vec2 position, vec2 size, f32 rotation_angle, vec3 color, f32 alpha, texture_t *texture)
{
    renderer_draw_quad_sprite_color_uv(position, size, rotation_angle, color, alpha, texture, renderer_data.default_quad_vertex_tex_coords);
}

void renderer_draw_quad_sprite_color_uv(vec2 position, vec2 size, f32 rotation_angle, vec3 color, f32 alpha, texture_t *texture, const vec2 uv[4])
{
    if (renderer_data.index_count >= RENDERER_MAX_INDEX_COUNT || renderer_data.texture_slot_index > 31) {
        next_batch();
    }

    f32 texture_index = 0.0f;
    for (u32 i = 0; i < renderer_data.texture_slot_index; i++) {
        if (renderer_data.texture_slots[i] == texture->id) {
            texture_index = (f32)i;
            break;
        }
    }

    if (texture_index == 0.0f) {
        texture_index = (f32)renderer_data.texture_slot_index;
        renderer_data.texture_slots[renderer_data.texture_slot_index] = texture->id;
        renderer_data.texture_slot_index++;
    }

    mat4 translation_matrix = mat4_translate(position);
    mat4 rotation_matrix = mat4_rotate(rotation_angle);
    mat4 scale_matrix = mat4_scale(size);
    mat4 model_matrix = mat4_multiply(translation_matrix, mat4_multiply(rotation_matrix, scale_matrix));

    for (i32 i = 0; i < QUAD_VERTEX_COUNT; i++) {
        renderer_data.quad_buffer_ptr->position = mat4_multiply_vec4(model_matrix, renderer_data.default_quad_vertex_positions[i]);
        renderer_data.quad_buffer_ptr->color = vec4_create(color.r, color.g, color.b, alpha);
        renderer_data.quad_buffer_ptr->tex_coords = uv[i];
        renderer_data.quad_buffer_ptr->tex_index = texture_index;
        renderer_data.quad_buffer_ptr++;
    }

    renderer_data.index_count += 6;
    renderer_stats.quad_count++;
}

void renderer_draw_text(const char *text, font_atlas_size_e fa_size, vec2 position, f32 scale, vec3 color, f32 alpha)
{
    ASSERT(text);
    ASSERT(fa_size < FA_COUNT);

    vec4 font_color = vec4_create_from_vec3(color, alpha);
    shader_bind(&renderer_data.font_shader);
    shader_set_uniform_int(&renderer_data.font_shader, "u_texture", 0);
    shader_set_uniform_vec4(&renderer_data.font_shader, "u_color", &font_color);

    i32 n = 0;
    f32 start_x = position.x;
    vertex_2d coords[6 * strlen(text)];
    memset(coords, 0, 6 * strlen(text));

    font_atlas_t *fa = &renderer_data.font_atlases[fa_size];
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
    glBindVertexArray(renderer_data.font_va);
    glBindBuffer(GL_ARRAY_BUFFER, renderer_data.font_vb);
    glBufferData(GL_ARRAY_BUFFER, sizeof(coords), coords, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, n);
}

u32 renderer_get_font_height(font_atlas_size_e fa)
{
    return renderer_data.font_atlases[fa].height;
}

u32 renderer_get_font_width(font_atlas_size_e fa)
{
    // NOTE: Works only for monospaced fonts
    return renderer_data.font_atlases[fa].glyphs[32].advance.x;
}

static void start_batch(void)
{
    renderer_data.index_count = 0;
    renderer_data.quad_buffer_ptr = renderer_data.quad_buffer;
}

static void next_batch(void)
{
    flush();
    start_batch();
}

static void flush(void)
{
    u32 size = (u32)((u8 *)renderer_data.quad_buffer_ptr - (u8 *)renderer_data.quad_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, renderer_data.quad_vb);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size, (const void *)renderer_data.quad_buffer);

    for (u32 i = 0; i < renderer_data.texture_slot_index; i++) {
        glBindTextureUnit(i, renderer_data.texture_slots[i]);
    }

    shader_bind(&renderer_data.quad_shader);
    glBindVertexArray(renderer_data.quad_va);
    glDrawElements(GL_TRIANGLES, renderer_data.index_count, GL_UNSIGNED_INT, NULL);

    renderer_stats.draw_calls++;
}

static void create_shaders(void)
{
    shader_create_info_t font_shader_create_info = {
        .vertex_filepath = "assets/shaders/text.vert",
        .fragment_filepath = "assets/shaders/text.frag"
    };

    if (!shader_create(&font_shader_create_info, &renderer_data.font_shader)) {
        LOG_ERROR("failed to create font shader");
    }

    shader_create_info_t quad_shader_create_info = {
        "assets/shaders/quad.vert",
        "assets/shaders/quad.frag"
    };

    if (!shader_create(&quad_shader_create_info, &renderer_data.quad_shader)) {
        LOG_ERROR("failed to create quad shader");
    }
}

static void create_font_data(void)
{
    glGenVertexArrays(1, &renderer_data.font_va);
    glBindVertexArray(renderer_data.font_va);

    glGenBuffers(1, &renderer_data.font_vb);
    glBindBuffer(GL_ARRAY_BUFFER, renderer_data.font_vb);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(vertex_2d), 0);

    if (FT_Init_FreeType(&ft)) {
        LOG_FATAL("failed to initialize freetype library");
    }

    if (FT_New_Face(ft, FONT_FILEPATH, 0, &face)) {
        LOG_ERROR("failed to load font at %s", FONT_FILEPATH);
    }

    create_font_atlas(face, 16, &renderer_data.font_atlases[FA16]);
    create_font_atlas(face, 32, &renderer_data.font_atlases[FA32]);
    create_font_atlas(face, 64, &renderer_data.font_atlases[FA64]);
    create_font_atlas(face, 128, &renderer_data.font_atlases[FA128]);
}

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
