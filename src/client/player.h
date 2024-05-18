#pragma once

#include "event.h"
#include "common/packet.h"
#include "common/player_types.h"

void player_load_animations(void);

void player_self_create(const char *name, packet_player_init_t *packet, player_self_t *out_self_player);
void player_remote_create(packet_player_add_t *packet, player_remote_t *out_remote_player);

void player_self_destroy(player_self_t *player);

void player_self_update(player_self_t *player, f64 delta_time);
void player_self_handle_authoritative_update(player_self_t *player, packet_player_update_t *packet);

void player_remote_handle_authoritative_update(player_remote_t *player, packet_player_update_t *packet);

void player_take_damage(player_base_t *player, u32 damage);

void player_self_render(player_self_t *player, f64 delta_time);
void player_remote_render(player_remote_t *player, f64 delta_time, f32 server_update_accumulator);

void player_respawn(player_base_t *player, packet_player_respawn_t *packet);

b8 player_key_pressed_event_callback(event_code_e code, event_data_t data);
b8 player_key_released_event_callback(event_code_e code, event_data_t data);
