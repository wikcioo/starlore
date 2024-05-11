#pragma once

#include "defines.h"
#include "common/packet.h"

typedef struct {
    char name[MAX_PLAYER_NAME_LENGTH];
    char content[MAX_MESSAGE_CONTENT_LENGTH];
} chat_player_message_t;

void chat_init(void);
void chat_shutdown(void);

void chat_add_player_message(chat_player_message_t message);
void chat_add_system_message(const char *message);

void chat_render(void);
