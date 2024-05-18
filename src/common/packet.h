#pragma once

#include "defines.h"
#include "common/maths.h"
#include "common/global.h"
#include "common/player_types.h"

typedef enum {
    PACKET_TYPE_NONE,
    PACKET_TYPE_HEADER,
    PACKET_TYPE_PING,
    PACKET_TYPE_MESSAGE,
    PACKET_TYPE_MESSAGE_HISTORY,
    PACKET_TYPE_PLAYER_INIT,
    PACKET_TYPE_PLAYER_INIT_CONF,
    PACKET_TYPE_PLAYER_ADD,
    PACKET_TYPE_PLAYER_REMOVE,
    PACKET_TYPE_PLAYER_UPDATE,
    PACKET_TYPE_PLAYER_HEALTH,
    PACKET_TYPE_PLAYER_DEATH,
    PACKET_TYPE_PLAYER_RESPAWN,
    PACKET_TYPE_PLAYER_KEYPRESS,
    PACKET_TYPE_COUNT
} packet_type_e;

typedef enum {
    MESSAGE_TYPE_NONE,
    MESSAGE_TYPE_SYSTEM,
    MESSAGE_TYPE_PLAYER,
    MESSAGE_TYPE_COUNT
} message_type_e;

typedef struct {
    u32 type;
    u32 size;
} packet_header_t;

typedef struct {
    u64 time;
} packet_ping_t;

typedef struct {
    u32 type;
    i64 timestamp;
    char author[PLAYER_MAX_NAME_LENGTH];
    char content[MESSAGE_MAX_CONTENT_LENGTH];
} packet_message_t;

typedef struct {
    u32 count;
    packet_message_t history[MAX_MESSAGE_HISTORY_LENGTH];
} packet_message_history_t;

typedef struct {
    player_id id;
    vec2 position;
    vec3 color;
    i32 health;
    player_state_e state;
    player_direction_e direction;
} packet_player_init_t;

typedef struct {
    player_id id;
    char name[PLAYER_MAX_NAME_LENGTH];
} packet_player_init_confirm_t;

typedef struct {
    player_id id;
    char name[PLAYER_MAX_NAME_LENGTH];
    vec2 position;
    vec3 color;
    i32 health;
    player_state_e state;
    player_direction_e direction;
} packet_player_add_t;

typedef struct {
    player_id id;
} packet_player_remove_t;

typedef struct {
    u32 seq_nr;
    player_id id;
    vec2 position;
    u8 direction;
    u8 state;
} packet_player_update_t;

typedef struct {
    player_id id;
    u32 damage;
} packet_player_health_t;

typedef struct {
    player_id id;
} packet_player_death_t;

typedef struct {
    player_id id;
    i32 health;
    vec2 position;
    player_state_e state;
    player_direction_e direction;
} packet_player_respawn_t;

typedef struct {
    player_id id;
    u32 seq_nr;
    u32 key;
    u32 mods;
    u32 action;
} packet_player_keypress_t;

static const u32 PACKET_TYPE_SIZE[PACKET_TYPE_COUNT] = {
    [PACKET_TYPE_NONE]              = 0,
    [PACKET_TYPE_HEADER]            = sizeof(packet_header_t),
    [PACKET_TYPE_PING]              = sizeof(packet_ping_t),
    [PACKET_TYPE_MESSAGE]           = sizeof(packet_message_t),
    [PACKET_TYPE_MESSAGE_HISTORY]   = sizeof(packet_message_history_t),
    [PACKET_TYPE_PLAYER_INIT]       = sizeof(packet_player_init_t),
    [PACKET_TYPE_PLAYER_INIT_CONF]  = sizeof(packet_player_init_confirm_t),
    [PACKET_TYPE_PLAYER_ADD]        = sizeof(packet_player_add_t),
    [PACKET_TYPE_PLAYER_REMOVE]     = sizeof(packet_player_remove_t),
    [PACKET_TYPE_PLAYER_UPDATE]     = sizeof(packet_player_update_t),
    [PACKET_TYPE_PLAYER_HEALTH]     = sizeof(packet_player_health_t),
    [PACKET_TYPE_PLAYER_DEATH]      = sizeof(packet_player_death_t),
    [PACKET_TYPE_PLAYER_RESPAWN]    = sizeof(packet_player_respawn_t),
    [PACKET_TYPE_PLAYER_KEYPRESS]   = sizeof(packet_player_keypress_t)
};

b8 packet_send(i32 socket, u32 type, void *packet_data);
u64 packet_get_next_sequence_number(void);
