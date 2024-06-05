#pragma once

#include "camera.h"
#include "common/packet.h"
#include "common/game_world_types.h"

void game_world_init(packet_game_world_init_t *packet, game_world_t *out_game_world);
void game_world_destroy(game_world_t *game_world);

void game_world_load_resources(game_world_t *game_world);
void game_world_add_objects(game_world_t *game_world, game_object_t *objects, u32 length);

void game_world_render(game_world_t *game_world);

void game_world_process_player_position(game_world_t *game_world, vec2 position);
