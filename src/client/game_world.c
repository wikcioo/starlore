#include "game_world.h"

#include <memory.h>
#include <stdlib.h>

#include "config.h"
#include "texture.h"
#include "renderer.h"
#include "common/logger.h"
#include "common/asserts.h"
#include "common/perlin_noise.h"
#include "common/containers/darray.h"

typedef u8 tile_type_t;

static i32 chunk_x = 0;
static i32 chunk_y = 0;
static tile_type_t *map_data;
static texture_t terrain_spritesheet;
static texture_t vegetation_spritesheet;

extern vec2 main_window_size;

#if defined(DEBUG)
texture_t perlin_noise_texture;
#endif
vec2 terrain_tex_coord[TILE_TYPE_COUNT][TEX_COORD_COUNT];
vec2 vegetation_tex_coord[GAME_OBJECT_TYPE_COUNT][TEX_COORD_COUNT];

static void load_tex_coord(vec2 tex_coord[][TEX_COORD_COUNT], u32 type, f32 x, f32 y, f32 width, f32 height);
static void reload_map_data(game_world_t *game_world);
static void load_textures(void);

void game_world_init(packet_game_world_init_t *packet, game_world_t *out_game_world)
{
    ASSERT(packet);
    ASSERT(out_game_world);

    memcpy(&out_game_world->map, &packet->map, sizeof(game_map_t));
    out_game_world->objects = darray_create(sizeof(game_object_t));
}

void game_world_destroy(game_world_t *game_world)
{
    darray_destroy(map_data);
    darray_destroy(game_world->objects);
    texture_destroy(&terrain_spritesheet);
    texture_destroy(&vegetation_spritesheet);
#if defined(DEBUG)
    texture_destroy(&perlin_noise_texture);
#endif
}

void game_world_load_resources(game_world_t *game_world)
{
    u32 map_width = game_world->map.width;
    u32 map_height = game_world->map.height;

    map_data = darray_reserve(map_width * map_height, sizeof(tile_type_t));

#if defined(DEBUG)
    texture_specification_t perlin_noise_texture_spec = {
        .width = map_width,
        .height = map_height,
        .format = IMAGE_FORMAT_R8,
        .generate_mipmaps = false
    };
    texture_create_from_spec(perlin_noise_texture_spec, NULL, &perlin_noise_texture, "perlin_noise");
#endif

    reload_map_data(game_world);
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

void game_world_render(game_world_t *game_world)
{
    ASSERT_MSG(map_data, "game_world_render called before map data loaded");

    u32 map_width = game_world->map.width;
    u32 map_height = game_world->map.height;

    vec2 map_start = vec2_create(
        -((game_world->map.width  / 2.0f) * TILE_WIDTH_PX ) + (TILE_WIDTH_PX  / 2.0f) + (chunk_x * TILE_WIDTH_PX ),
        -((game_world->map.height / 2.0f) * TILE_HEIGHT_PX) + (TILE_HEIGHT_PX / 2.0f) + (chunk_y * TILE_HEIGHT_PX)
    );

    // Render tilemap
    for (u32 row = 0; row < map_height; row++) {
        for (u32 col = 0; col < map_width; col++) {
            u32 idx = row * map_width + col;
            tile_type_t tile_type = map_data[idx];

            vec2 position = vec2_create(
                map_start.x + (col * TILE_WIDTH_PX),
                map_start.y + (row * TILE_HEIGHT_PX)
            );

            vec2 size = vec2_create(TILE_WIDTH_PX, TILE_HEIGHT_PX);

            vec2 tex_coord[4] = {0};
            memcpy(tex_coord, &terrain_tex_coord[tile_type], sizeof(vec2) * TEX_COORD_COUNT);

            renderer_draw_quad_sprite_uv(position, size, 0.0f, &terrain_spritesheet, tex_coord);
        }
    }

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
        memcpy(tex_coord, &vegetation_tex_coord[game_world->objects[i].type], sizeof(vec2) * TEX_COORD_COUNT);

        renderer_draw_quad_sprite_uv(position, size, 0.0f, &vegetation_spritesheet, tex_coord);
    }
#endif
}

void game_world_process_player_position(game_world_t *game_world, vec2 position)
{
    i32 new_chunk_x = position.x / TILE_WIDTH_PX;
    i32 new_chunk_y = position.y / TILE_HEIGHT_PX;

    if (new_chunk_x != chunk_x || new_chunk_y != chunk_y) {
        chunk_x = new_chunk_x;
        chunk_y = new_chunk_y;
        reload_map_data(game_world);
    }
}

static void load_tex_coord(vec2 tex_coord[][TEX_COORD_COUNT], u32 type, f32 x, f32 y, f32 width, f32 height)
{
    tex_coord[type][0] = vec2_create(x, y);
    tex_coord[type][1] = vec2_create(x, y + height);
    tex_coord[type][2] = vec2_create(x + width, y + height);
    tex_coord[type][3] = vec2_create(x + width, y);
}

static void reload_map_data(game_world_t *game_world)
{
    u32 map_width = game_world->map.width;
    u32 map_height = game_world->map.height;

    f32 *perlin_noise_data = malloc(map_width * map_height * sizeof(f32));

    perlin_noise_config_t config = {
        .pos_x = chunk_x,
        .pos_y = chunk_y,
        .width = map_width,
        .height = map_height,
        .seed = game_world->map.seed,
        .octave_count = game_world->map.octave_count,
        .scaling_bias = game_world->map.bias
    };

    perlin_noise_generate_2d(config, perlin_noise_data);

    for (u32 row = 0; row < map_height; row++) {
        for (u32 col = 0; col < map_width; col++) {
            u32 idx = row * map_width + col;

            tile_type_t tile_type = TILE_TYPE_NONE;
            f32 value = perlin_noise_data[idx];
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
            map_data[idx] = tile_type;
        }
    }

#if defined(DEBUG)
    u8 *perlin_noise_color = malloc(map_width * map_height * sizeof(u8));
    for (u32 i = 0; i < map_width * map_height; i++) {
        perlin_noise_color[i] = (u8)(perlin_noise_data[i] * 255.0f);
    }

    texture_set_data(&perlin_noise_texture, perlin_noise_color);

    free(perlin_noise_color);
#endif

    free(perlin_noise_data);
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
