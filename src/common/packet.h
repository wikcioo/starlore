#pragma once

#include "defines.h"
#include "common/maths.h"

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
    PACKET_TYPE_PLAYER_KEYPRESS,
    PACKET_TYPE_COUNT
} packet_type_e;

typedef enum {
    MESSAGE_TYPE_NONE,
    MESSAGE_TYPE_SYSTEM,
    MESSAGE_TYPE_PLAYER,
    MESSAGE_TYPE_COUNT
} message_type_e;

#define MAX_PLAYER_NAME_LENGTH 32
#define MAX_MESSAGE_CONTENT_LENGTH 256
#define MAX_PLAYER_COUNT 5
#define MAX_MESSAGE_HISTORY_LENGTH 8

#define PLAYER_INVALID_ID 0

typedef u32 player_id;

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
    char author[MAX_PLAYER_NAME_LENGTH];
    char content[MAX_MESSAGE_CONTENT_LENGTH];
} packet_message_t;

typedef struct {
    u32 count;
    packet_message_t history[MAX_MESSAGE_HISTORY_LENGTH];
} packet_message_history_t;

typedef struct {
    player_id id;
    vec2 position;
    vec3 color;
} packet_player_init_t;

typedef struct {
    player_id id;
    char name[MAX_PLAYER_NAME_LENGTH];
} packet_player_init_confirm_t;

typedef struct {
    player_id id;
    char name[MAX_PLAYER_NAME_LENGTH];
    vec2 position;
    vec3 color;
} packet_player_add_t;

typedef struct {
    player_id id;
} packet_player_remove_t;

typedef struct {
    u32 seq_nr;
    player_id id;
    vec2 position;
} packet_player_update_t;

typedef struct {
    player_id id;
    u32 seq_nr;
    i32 key;
    i32 action;
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
    [PACKET_TYPE_PLAYER_KEYPRESS]   = sizeof(packet_player_keypress_t)
};

b8 packet_send(i32 socket, u32 type, void *packet_data);
