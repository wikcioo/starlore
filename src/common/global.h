#pragma once

#include "defines.h"
#include "common/game_world_types.h"

typedef u32 player_id;
typedef u8 tile_type_t;

#define CLIENT_TICK_RATE 64
#define CLIENT_TICK_DURATION (1.0f / CLIENT_TICK_RATE)
#define SERVER_TICK_RATE 64

#define MAX_PLAYER_COUNT 5
#define MAX_MESSAGE_HISTORY_LENGTH 8

#define PLAYER_INVALID_ID 0
#define PLAYER_MAX_NAME_LENGTH 32
#define PLAYER_VELOCITY 300.0f
#define PLAYER_DAMAGE_VALUE 10.0f
#define PLAYER_START_HEALTH 200.0f

#define PLAYER_ATTACK_COOLDOWN 1.0f
#define PLAYER_ATTACK_DURATION 0.3f

#define PLAYER_ROLL_COOLDOWN 1.0f
#define PLAYER_ROLL_DURATION 0.4f
#define PLAYER_ROLL_DISTANCE 250.0f

#define PLAYER_RESPAWN_COOLDOWN 5.0f

#define INVENTORY_MAX_QUICK_ACCESS_ITEMS 8
#define INVENTORY_MAX_ITEM_NAME_LENGTH   32
#define INVENTORY_INITIAL_CAPACITY       16

#define MESSAGE_MAX_CONTENT_LENGTH 256

#define CHUNK_LENGTH    16
#define CHUNK_WIDTH_PX  (CHUNK_LENGTH * TILE_WIDTH_PX)
#define CHUNK_HEIGHT_PX (CHUNK_LENGTH * TILE_HEIGHT_PX)
#define CHUNK_NUM_TILES (CHUNK_LENGTH * CHUNK_LENGTH)

#define INVALID_OBJECT_INDEX -1

typedef struct {
    i32 x, y;
    game_tile_t tiles[CHUNK_NUM_TILES];
    game_object_t objects[CHUNK_NUM_TILES];
#if defined(DEBUG)
    f32 noise_data[CHUNK_NUM_TILES];
#endif
} chunk_base_t;
