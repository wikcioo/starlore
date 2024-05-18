#pragma once

#include "defines.h"
#include "event.h"
#include "common/global.h"

typedef struct {
    char name[PLAYER_MAX_NAME_LENGTH];
    char content[MESSAGE_MAX_CONTENT_LENGTH];
} chat_player_message_t;

void chat_init(void);
void chat_shutdown(void);

b8 chat_key_pressed_event_callback(event_code_e code, event_data_t data);
b8 chat_key_repeated_event_callback(event_code_e code, event_data_t data);
b8 chat_char_pressed_event_callback(event_code_e code, event_data_t data);
b8 chat_mouse_button_pressed_event_callback(event_code_e code, event_data_t data);

void chat_add_player_message(chat_player_message_t message);
void chat_add_system_message(const char *message);

void chat_render(void);
