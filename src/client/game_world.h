#pragma once

#include "camera.h"
#include "common/packet.h"
#include "common/game_world_types.h"

void game_world_init(packet_game_world_init_t *packet, game_world_t *out_game_world);
void game_world_destroy(game_world_t *game_world);

void game_world_load_resources(game_world_t *game_world);
void game_world_add_chunk(chunk_base_t *chunk, const camera_t *const camera);
void game_world_render(game_world_t *game_world, const camera_t *const camera);

u64 game_world_get_chunk_num(void);
u64 game_world_get_chunk_size(void);

b8 game_world_key_pressed_event_callback(event_code_e code, event_data_t data);
