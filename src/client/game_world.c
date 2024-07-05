#include "game_world.h"

#include <stdio.h>
#include <memory.h>
#include <stdlib.h>

#include "config.h"
#include "texture.h"
#include "renderer.h"
#include "color_palette.h"
#include "common/logger.h"
#include "common/asserts.h"
#include "common/input_codes.h"
#include "common/perlin_noise.h"
#include "common/memory/memutils.h"
#include "common/containers/darray.h"

#define CHUNK_WIDTH_PX  (CHUNK_LENGTH * TILE_WIDTH_PX)
#define CHUNK_HEIGHT_PX (CHUNK_LENGTH * TILE_HEIGHT_PX)
#define CHUNK_NUM_TILES (CHUNK_LENGTH * CHUNK_LENGTH)

typedef u8 tile_type_t;

typedef struct {
    i32 age;
    i32 x, y;
    tile_type_t tiles[CHUNK_NUM_TILES];
#if defined(DEBUG)
    texture_t perlin_noise_texture;
#endif
} chunk_t;

static chunk_t *chunks;
static texture_t terrain_spritesheet;
static texture_t vegetation_spritesheet;

#if defined(DEBUG)
static b8 show_grid_coords = false;
static b8 show_perlin_noise_textures = false;
#endif

extern vec2 main_window_size;

vec2 terrain_tex_coord[TILE_TYPE_COUNT][TEX_COORD_COUNT];
vec2 vegetation_tex_coord[GAME_OBJECT_TYPE_COUNT][TEX_COORD_COUNT];

static void load_tex_coord(vec2 tex_coord[][TEX_COORD_COUNT], u32 type, f32 x, f32 y, f32 width, f32 height);
static void load_textures(void);

void game_world_init(packet_game_world_init_t *packet, game_world_t *out_game_world)
{
    ASSERT(packet);
    ASSERT(out_game_world);

    mem_copy(&out_game_world->map, &packet->map, sizeof(game_map_t));
    out_game_world->objects = darray_create(sizeof(game_object_t));
}

void game_world_destroy(game_world_t *game_world)
{
#if defined(DEBUG)
    u64 chunks_length = darray_length(chunks);
    for (u64 i = 0; i < chunks_length; i++) {
        texture_destroy(&chunks[i].perlin_noise_texture);
    }
#endif

    darray_destroy(chunks);
    darray_destroy(game_world->objects);
    texture_destroy(&terrain_spritesheet);
    texture_destroy(&vegetation_spritesheet);
}

void game_world_load_resources(game_world_t *game_world)
{
    chunks = darray_create(sizeof(chunk_t));
    load_textures();
}

void game_world_add_objects(game_world_t *game_world, game_object_t *objects, u32 length)
{
    ASSERT(game_world);
    ASSERT(objects);
    ASSERT(length > 0);

    for (u32 i = 0; i < length; i++) {
        darray_push(game_world->objects, objects[i]);
    }
}

static void game_world_render_chunk(chunk_t *chunk, i32 x, i32 y)
{
#if defined(DEBUG)
        if (show_perlin_noise_textures) {
            vec2 position = vec2_create(x * CHUNK_WIDTH_PX, y * CHUNK_HEIGHT_PX);
            static const vec2 size = {{ CHUNK_WIDTH_PX, CHUNK_HEIGHT_PX }};
            renderer_draw_quad_sprite(position, size, 0.0f, &chunk->perlin_noise_texture);
        } else {
#endif

    vec2 chunk_top_left_tile_pos = vec2_create(
        (x * CHUNK_WIDTH_PX ) - (CHUNK_WIDTH_PX /2) + (TILE_WIDTH_PX /2),
        (y * CHUNK_HEIGHT_PX) - (CHUNK_HEIGHT_PX/2) + (TILE_HEIGHT_PX/2)
    );

    for (u32 j = 0; j < CHUNK_NUM_TILES; j++) {
        u32 col = j % CHUNK_LENGTH;
        u32 row = j / CHUNK_LENGTH;
        tile_type_t tile_type = chunk->tiles[j];

        vec2 position = vec2_create(
            chunk_top_left_tile_pos.x + (col * TILE_WIDTH_PX),
            chunk_top_left_tile_pos.y + (row * TILE_HEIGHT_PX)
        );

        static const vec2 size = {{ TILE_WIDTH_PX, TILE_HEIGHT_PX }};

        vec2 tex_coord[4] = {0};
        mem_copy(tex_coord, &terrain_tex_coord[tile_type], sizeof(vec2) * TEX_COORD_COUNT);

        renderer_draw_quad_sprite_uv(position, size, 0.0f, &terrain_spritesheet, tex_coord);
    }

#if defined(DEBUG)
        }
#endif
}

static void game_world_load_chunk(game_world_t *game_world, i32 x, i32 y, chunk_t **out_chunk)
{
    f32 *perlin_noise_data = mem_alloc(CHUNK_NUM_TILES * sizeof(f32), MEMORY_TAG_GAME);

    perlin_noise_config_t config = {
        .pos_x = x * CHUNK_LENGTH,
        .pos_y = y * CHUNK_LENGTH,
        .width = CHUNK_LENGTH,
        .height = CHUNK_LENGTH,
        .seed = game_world->map.seed,
        .octave_count = game_world->map.octave_count,
        .scaling_bias = game_world->map.bias
    };

    perlin_noise_generate_2d(config, perlin_noise_data);

    chunk_t new_chunk = {0};
    new_chunk.age = 0;
    new_chunk.x = x;
    new_chunk.y = y;

    for (u32 i = 0; i < CHUNK_NUM_TILES; i++) {
        tile_type_t tile_type = TILE_TYPE_NONE;
        f32 value = perlin_noise_data[i];
        if (value < 0.4f) {
            tile_type = TILE_TYPE_WATER;
        } else if (value < 0.45f) {
            tile_type = TILE_TYPE_DIRT;
        } else if (value < 0.8f) {
            tile_type = TILE_TYPE_GRASS;
        } else {
            tile_type = TILE_TYPE_STONE;
        }

        ASSERT(tile_type > TILE_TYPE_NONE && tile_type < TILE_TYPE_COUNT);
        new_chunk.tiles[i] = tile_type;
    }

#if defined(DEBUG)
    u8 *perlin_noise_color = mem_alloc(CHUNK_NUM_TILES * sizeof(u8), MEMORY_TAG_GAME);
    
    for (u32 i = 0; i < CHUNK_NUM_TILES; i++) {
        perlin_noise_color[i] = (u8)(perlin_noise_data[i] * 255.0f);
    }

    texture_specification_t perlin_noise_texture_spec = {
        .width = CHUNK_LENGTH,
        .height = CHUNK_LENGTH,
        .format = IMAGE_FORMAT_R8,
        .generate_mipmaps = false
    };

    char name[32] = {0};
    snprintf(name, sizeof(name), "perlin_noise (%i:%i)", x, y);
    texture_create_from_spec(perlin_noise_texture_spec, perlin_noise_color, &new_chunk.perlin_noise_texture, name);

    mem_free(perlin_noise_color, CHUNK_NUM_TILES * sizeof(u8), MEMORY_TAG_GAME);
#endif

    u64 chunks_length = darray_length(chunks);
    if (chunks_length >= CHUNK_CACHE_MAX_ITEMS) {
        // Crossed allowed cache size - start overriding the oldest chunks in cache
        u64 oldest = 0;
        u64 oldest_idx = 0;
        for (u64 i = 0; i < chunks_length; i++) {
            if (chunks[i].age > oldest) {
                oldest_idx = i;
                oldest = chunks[i].age;
            }
        }

        chunk_t *oldest_chunk = &chunks[oldest_idx];

#if LOG_REACH_CHUNK_CACHE_SIZE_LIMIT
        LOG_TRACE("reached chunk cache size limit of %u items", CHUNK_CACHE_MAX_ITEMS);
        LOG_TRACE("overriding oldest chunk in cache at index %llu (age: %i, coords: %i:%i)",
                  oldest_idx, oldest_chunk->age, oldest_chunk->x, oldest_chunk->y);
#endif

#if defined(DEBUG)
        texture_destroy(&oldest_chunk->perlin_noise_texture);
#endif
        mem_copy(oldest_chunk, &new_chunk, sizeof(chunk_t));
        *out_chunk = oldest_chunk;
    } else {
        // Append new chunk to cache
        darray_push(chunks, new_chunk);
        *out_chunk = &chunks[chunks_length-1];
    }

    mem_free(perlin_noise_data, CHUNK_NUM_TILES * sizeof(f32), MEMORY_TAG_GAME);
}

void game_world_render(game_world_t *game_world, const camera_t *const camera)
{
    f32 cx = camera->position.x;
    f32 cy = camera->position.y;
    f32 ww = main_window_size.x;
    f32 wh = main_window_size.y;

    f32 lp = cx - ww/2;
    f32 rp = cx + ww/2;
    f32 tp = cy + wh/2;
    f32 bp = cy - wh/2;
    i32 left_coord   = (i32)((lp + (math_sign(lp) * CHUNK_WIDTH_PX/2))  / CHUNK_WIDTH_PX);
    i32 right_coord  = (i32)((rp + (math_sign(rp) * CHUNK_WIDTH_PX/2))  / CHUNK_WIDTH_PX);
    i32 top_coord    = (i32)((tp + (math_sign(tp) * CHUNK_HEIGHT_PX/2)) / CHUNK_HEIGHT_PX);
    i32 bottom_coord = (i32)((bp + (math_sign(bp) * CHUNK_HEIGHT_PX/2)) / CHUNK_HEIGHT_PX);

    for (i32 y = top_coord; y >= bottom_coord; y--) {
        for (i32 x = left_coord; x <= right_coord; x++) {
            // NOTE: Possible to optimize the lookup using a hashtable, if the cache gets large enough

            // Check if x:y chunk exists
            u64 chunks_length = darray_length(chunks);
            chunk_t *chunk = NULL;
            for (u64 i = 0; i < chunks_length; i++) {
                chunk_t *c = &chunks[i];
                if (c->x == x && c->y == y) {
                    c->age = 0;
                    chunk = c;
                    break;
                }
            }

            if (chunk == NULL) {
                // Generate new chunk and insert it into array of chunks
                game_world_load_chunk(game_world, x, y, &chunk);

                // Increment age of chunks, which are not visible
                chunks_length = darray_length(chunks);
                for (u64 i = 0; i < chunks_length; i++) {
                    chunk_t *c = &chunks[i];
                    if (c->x < left_coord || c->x > right_coord || c->y > top_coord || c->y < bottom_coord) {
                        c->age++;
                    }
                }
            }

            ASSERT(chunk != NULL);
            game_world_render_chunk(chunk, x, y);
        }
    }

#if defined(DEBUG)
    if (show_grid_coords) {
        u64 chunks_length = darray_length(chunks);
        for (u64 i = 0; i < chunks_length; i++) {
            chunk_t *chunk = &chunks[i];
            i32 x = chunk->x;
            i32 y = chunk->y;

            vec2 pos = vec2_create(x * CHUNK_WIDTH_PX, y * CHUNK_HEIGHT_PX);
            renderer_draw_quad_color(pos, vec2_create(CHUNK_WIDTH_PX, CHUNK_HEIGHT_PX), 0.0f, COLOR_BLACK, 0.3f);
            renderer_draw_rect(pos, vec2_create(CHUNK_WIDTH_PX, CHUNK_HEIGHT_PX), COLOR_CRIMSON_RED, 1.0f);

            f32 padding = 10.0f;
            u32 font_height = renderer_get_font_height(FA64);
            pos.x -= (CHUNK_WIDTH_PX/2 - padding);
            pos.y += (CHUNK_HEIGHT_PX/2 - padding - font_height);

            char buffer[256] = {0};
            snprintf(buffer, sizeof(buffer), "cached chunk\nage: %i\ncoords: %i:%i", chunk->age, x, y);
            renderer_draw_text(buffer, FA64, pos, 1.0f, COLOR_MILK, 1.0f);
        }
    }
#endif

#if 0
    // Render game objects
    u64 num_objects = darray_length(game_world->objects);
    for (u64 i = 0; i < num_objects; i++) {
        i32 idx = game_world->objects[i].tile_index;
        u32 row = idx / map_width;
        u32 col = idx % map_width;

        vec2 position = vec2_create(
            map_start.x + (col * TILE_WIDTH_PX),
            map_start.y + (row * TILE_HEIGHT_PX)
        );

        vec2 size = vec2_create(TILE_WIDTH_PX, TILE_HEIGHT_PX);

        vec2 tex_coord[4] = {0};
        mem_copy(tex_coord, &vegetation_tex_coord[game_world->objects[i].type], sizeof(vec2) * TEX_COORD_COUNT);

        renderer_draw_quad_sprite_uv(position, size, 0.0f, &vegetation_spritesheet, tex_coord);
    }
#endif
}

u64 game_world_get_chunk_num(void)
{
    return darray_length(chunks);
}

u64 game_world_get_chunk_size(void)
{
    return sizeof(chunk_t);
}

b8 game_world_key_pressed_event_callback(event_code_e code, event_data_t data)
{
#if defined(DEBUG)
    u16 key = data.u16[0];
    if (key == KEYCODE_G) {
        show_grid_coords = !show_grid_coords;
        return true;
    } else if (key == KEYCODE_N) {
        show_perlin_noise_textures = !show_perlin_noise_textures;
        return true;
    }
#endif

    return false;
}

static void load_tex_coord(vec2 tex_coord[][TEX_COORD_COUNT], u32 type, f32 x, f32 y, f32 width, f32 height)
{
    tex_coord[type][0] = vec2_create(x, y);
    tex_coord[type][1] = vec2_create(x, y + height);
    tex_coord[type][2] = vec2_create(x + width, y + height);
    tex_coord[type][3] = vec2_create(x + width, y);
}

static void load_textures(void)
{
    f32 x, y;

    // Terrain textures
    texture_create_from_path("assets/textures/world/v1/terrain.png", &terrain_spritesheet);
    f32 tile_width_uv = (f32)TILE_WIDTH_PX / (f32)terrain_spritesheet.width;
    f32 tile_height_uv = (f32)TILE_HEIGHT_PX / (f32)terrain_spritesheet.height;

    x = 1  * tile_width_uv;
    y = 10 * tile_height_uv;
    load_tex_coord(terrain_tex_coord, TILE_TYPE_GRASS, x, y, tile_width_uv, tile_height_uv);

    x = 1  * tile_width_uv;
    y = 14 * tile_height_uv;
    load_tex_coord(terrain_tex_coord, TILE_TYPE_DIRT, x, y, tile_width_uv, tile_height_uv);

    x = 0 * tile_width_uv;
    y = 3 * tile_height_uv;
    load_tex_coord(terrain_tex_coord, TILE_TYPE_STONE, x, y, tile_width_uv, tile_height_uv);

    x = 3 * tile_width_uv;
    y = 0 * tile_height_uv;
    load_tex_coord(terrain_tex_coord, TILE_TYPE_WATER, x, y, tile_width_uv, tile_height_uv);

    // Vegetation textures
    texture_create_from_path("assets/textures/world/v1/vegetation_and_other.png", &vegetation_spritesheet);
    f32 px_w_to_uv = 1.0f / vegetation_spritesheet.width;
    f32 px_h_to_uv = 1.0f / vegetation_spritesheet.height;

    f32 w, h;
    u32 tree_width_px = 64;
    u32 tree_height_px = 96;
    u32 tree_xoffset_px = 64;
    u32 tree_yoffset_px = vegetation_spritesheet.height - tree_height_px;

    x = tree_xoffset_px * px_w_to_uv;
    y = tree_yoffset_px * px_h_to_uv;
    w = tree_width_px * px_w_to_uv;
    h = tree_height_px * px_h_to_uv;
    load_tex_coord(vegetation_tex_coord, GAME_OBJECT_TYPE_TREE, x, y, w, h);

    u32 bush_width_px = 40;
    u32 bush_height_px = 34;
    u32 bush_xoffset_px = 0;
    u32 bush_yoffset_px = vegetation_spritesheet.height - (150 + bush_height_px);

    x = bush_xoffset_px * px_w_to_uv;
    y = bush_yoffset_px * px_h_to_uv;
    w = bush_width_px * px_w_to_uv;
    h = bush_height_px * px_h_to_uv;
    load_tex_coord(vegetation_tex_coord, GAME_OBJECT_TYPE_BUSH, x, y, w, h);

    u32 lily_width_px = 19;
    u32 lily_height_px = 17;
    u32 lily_xoffset_px = 112;
    u32 lily_yoffset_px = vegetation_spritesheet.height - (204 + lily_height_px);

    x = lily_xoffset_px * px_w_to_uv;
    y = lily_yoffset_px * px_h_to_uv;
    w = lily_width_px * px_w_to_uv;
    h = lily_height_px * px_h_to_uv;
    load_tex_coord(vegetation_tex_coord, GAME_OBJECT_TYPE_LILY, x, y, w, h);
}
