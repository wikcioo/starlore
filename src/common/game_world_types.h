#pragma once

#include "defines.h"
#include "common/maths.h"
#include "common/containers/darray.h"

typedef enum {
    GAME_OBJECT_TYPE_NONE,
    GAME_OBJECT_TYPE_TREE,
    GAME_OBJECT_TYPE_BUSH,
    GAME_OBJECT_TYPE_ROCK,
    GAME_OBJECT_TYPE_LILY,
    GAME_OBJECT_TYPE_COUNT
} game_object_type_e;

typedef enum {
    TILE_TYPE_NONE,
    TILE_TYPE_GRASS,
    TILE_TYPE_DIRT,
    TILE_TYPE_STONE,
    TILE_TYPE_WATER,
    TILE_TYPE_COUNT
} tile_type_e;

typedef struct {
    game_object_type_e type;
    i32 tile_index;
} game_object_t;

typedef struct {
    u32 seed;
    i32 octave_count;
    f32 bias;
} game_map_t;

typedef struct {
    game_map_t map;
    game_object_t *objects;
} game_world_t;
