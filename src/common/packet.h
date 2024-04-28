#pragma once

#include "defines.h"

#define PACKET_TYPE_NONE 0
#define PACKET_TYPE_PING 1
#define PACKET_TYPE_MESSAGE 2

#define MAX_AUTHOR_SIZE 32
#define MAX_CONTENT_SIZE 256

typedef struct packet_header
{
    u32 type;
    u32 size;
} packet_header_t;

typedef struct packet_ping
{
    u64 time;
} packet_ping_t;

typedef struct packet_message
{
    i64 timestamp;
    char author[MAX_AUTHOR_SIZE];
    char content[MAX_CONTENT_SIZE];
} packet_message_t;
