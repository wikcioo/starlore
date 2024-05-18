#pragma once

#include "defines.h"
#include "common/maths.h"
#include "common/global.h"

typedef enum {
    PLAYER_STATE_IDLE,
    PLAYER_STATE_WALK,
    PLAYER_STATE_ATTACK,
    PLAYER_STATE_ROLL,
    PLAYER_STATE_BLOCK,
    PLAYER_STATE_DEAD,
    PLAYER_STATE_COUNT
} player_state_e;

typedef enum {
    PLAYER_DIRECTION_DOWN,
    PLAYER_DIRECTION_LEFT,
    PLAYER_DIRECTION_RIGHT,
    PLAYER_DIRECTION_UP,
    PLAYER_DIRECTION_COUNT
} player_direction_e;

typedef struct {
    player_id id;
    char name[PLAYER_MAX_NAME_LENGTH];
    vec2 position;
    vec3 color;
    i32 health;
    player_state_e state;
    player_direction_e direction;
    struct {
        struct {
            u8 keyframe_index;
            f32 accumulator;
        } player;
        struct {
            b8 is_damaged;
            f32 accumulator;
        } damage;
    } animation;
} player_base_t;

typedef struct {
    player_base_t base;
    void *keypress_ring_buffer;
} player_self_t;

typedef struct {
    player_base_t base;
    vec2 last_position;
    b8 is_interpolated;
    f32 no_update_accumulator;
} player_remote_t;
