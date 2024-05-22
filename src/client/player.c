#include "player.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "window.h"
#include "texture.h"
#include "renderer.h"
#include "color_palette.h"
#include "common/global.h"
#include "common/asserts.h"
#include "common/logger.h"
#include "common/input_codes.h"
#include "common/containers/ring_buffer.h"

#define PLAYER_ANIMATION_KEYFRAME_COUNT 4
#define PLAYER_ANIMATION_FRAME_DURATION (1.0f / PLAYER_ANIMATION_FPS)

#define TEX_COORD_COUNT 4
#define KEYFRAME_WIDTH_PX 32
#define KEYFRAME_HEIGHT_PX 32

extern i32 client_socket;
extern camera_t game_camera;
extern b8 is_camera_locked_on_player;

static b8 player_keys_state[KEYCODE_Last];
static player_self_t *player_self_ref;
static texture_t player_animation_spritesheet;

vec2 idle_tex_coord   [PLAYER_DIRECTION_COUNT][PLAYER_ANIMATION_KEYFRAME_COUNT][TEX_COORD_COUNT];
vec2 walk_tex_coord   [PLAYER_DIRECTION_COUNT][PLAYER_ANIMATION_KEYFRAME_COUNT][TEX_COORD_COUNT];
vec2 attack_tex_coord [PLAYER_DIRECTION_COUNT][PLAYER_ANIMATION_KEYFRAME_COUNT][TEX_COORD_COUNT];
vec2 roll_tex_coord   [PLAYER_DIRECTION_COUNT][PLAYER_ANIMATION_KEYFRAME_COUNT][TEX_COORD_COUNT];
vec2 block_tex_coord  [PLAYER_DIRECTION_COUNT][TEX_COORD_COUNT];
vec2 dead_tex_coord   [PLAYER_ANIMATION_KEYFRAME_COUNT][TEX_COORD_COUNT];

void player_load_animations(void)
{
    texture_create_from_path("assets/textures/animation/player_spritesheet.png", &player_animation_spritesheet);

    f32 keyframe_width_uv = (f32)KEYFRAME_WIDTH_PX / player_animation_spritesheet.width;
    f32 keyframe_height_uv = (f32)KEYFRAME_HEIGHT_PX / player_animation_spritesheet.height;

    u32 row = 4;
    for (u32 dir = 0; dir < PLAYER_DIRECTION_COUNT; dir++) {
        for (u32 col = 0; col < PLAYER_ANIMATION_KEYFRAME_COUNT; col++) {
            f32 x = ((dir * PLAYER_DIRECTION_COUNT) + col) * keyframe_width_uv;
            f32 y = row * keyframe_height_uv;
            idle_tex_coord[dir][col][0] = vec2_create(x, y);
            idle_tex_coord[dir][col][1] = vec2_create(x, y + keyframe_height_uv);
            idle_tex_coord[dir][col][2] = vec2_create(x + keyframe_width_uv, y + keyframe_height_uv);
            idle_tex_coord[dir][col][3] = vec2_create(x + keyframe_width_uv, y);
        }
    }

    row--;

    for (u32 dir = 0; dir < PLAYER_DIRECTION_COUNT; dir++) {
        for (u32 col = 0; col < PLAYER_ANIMATION_KEYFRAME_COUNT; col++) {
            f32 x = ((dir * PLAYER_DIRECTION_COUNT) + col) * keyframe_width_uv;
            f32 y = row * keyframe_height_uv;
            walk_tex_coord[dir][col][0] = vec2_create(x, y);
            walk_tex_coord[dir][col][1] = vec2_create(x, y + keyframe_height_uv);
            walk_tex_coord[dir][col][2] = vec2_create(x + keyframe_width_uv, y + keyframe_height_uv);
            walk_tex_coord[dir][col][3] = vec2_create(x + keyframe_width_uv, y);
        }
    }

    row--;

    for (u32 dir = 0; dir < PLAYER_DIRECTION_COUNT; dir++) {
        for (u32 col = 0; col < PLAYER_ANIMATION_KEYFRAME_COUNT; col++) {
            f32 x = ((dir * PLAYER_DIRECTION_COUNT) + col) * keyframe_width_uv;
            f32 y = row * keyframe_height_uv;
            attack_tex_coord[dir][col][0] = vec2_create(x, y);
            attack_tex_coord[dir][col][1] = vec2_create(x, y + keyframe_height_uv);
            attack_tex_coord[dir][col][2] = vec2_create(x + keyframe_width_uv, y + keyframe_height_uv);
            attack_tex_coord[dir][col][3] = vec2_create(x + keyframe_width_uv, y);
        }
    }

    row--;

    for (u32 dir = 0; dir < PLAYER_DIRECTION_COUNT; dir++) {
        for (u32 col = 0; col < PLAYER_ANIMATION_KEYFRAME_COUNT; col++) {
            f32 x = ((dir * PLAYER_DIRECTION_COUNT) + col) * keyframe_width_uv;
            f32 y = row * keyframe_height_uv;
            roll_tex_coord[dir][col][0] = vec2_create(x, y);
            roll_tex_coord[dir][col][1] = vec2_create(x, y + keyframe_height_uv);
            roll_tex_coord[dir][col][2] = vec2_create(x + keyframe_width_uv, y + keyframe_height_uv);
            roll_tex_coord[dir][col][3] = vec2_create(x + keyframe_width_uv, y);
        }
    }

    row--;

    for (u32 dir = 0; dir < PLAYER_DIRECTION_COUNT; dir++) {
        f32 x = dir * keyframe_width_uv;
        f32 y = row * keyframe_height_uv;
        block_tex_coord[dir][0] = vec2_create(x, y);
        block_tex_coord[dir][1] = vec2_create(x, y + keyframe_height_uv);
        block_tex_coord[dir][2] = vec2_create(x + keyframe_width_uv, y + keyframe_height_uv);
        block_tex_coord[dir][3] = vec2_create(x + keyframe_width_uv, y);
    }

    for (u32 col = 0; col < PLAYER_ANIMATION_KEYFRAME_COUNT; col++) {
        f32 x = (col + 4) * keyframe_width_uv;
        f32 y = row * keyframe_height_uv;
        dead_tex_coord[col][0] = vec2_create(x, y);
        dead_tex_coord[col][1] = vec2_create(x, y + keyframe_height_uv);
        dead_tex_coord[col][2] = vec2_create(x + keyframe_width_uv, y + keyframe_height_uv);
        dead_tex_coord[col][3] = vec2_create(x + keyframe_width_uv, y);
    }
}

static void player_reset_player_animation(player_base_t *player)
{
    player->animation.player.keyframe_index = 0;
    player->animation.player.accumulator = 0.0f;
}

static void player_base_create(player_id id, const char *name, vec2 position, vec3 color, i32 health,
                               player_state_e state, player_direction_e direction, player_base_t *out_player_base)
{
    ASSERT(name);
    ASSERT(out_player_base);

    out_player_base->id = id;
    memcpy(out_player_base->name, name, strlen(name));
    out_player_base->position = position;
    out_player_base->color = color;
    out_player_base->health = health;
    out_player_base->state = state;
    out_player_base->direction = direction;
}

void player_self_create(const char *name, packet_player_init_t *packet, player_self_t *out_player_self)
{
    ASSERT(out_player_self);

    memset(out_player_self, 0, sizeof(player_self_t));
    player_base_create(packet->id, name, packet->position, packet->color, packet->health,
                       packet->state, packet->direction, &out_player_self->base);

    out_player_self->keypress_ring_buffer = ring_buffer_reserve(PLAYER_KEYPRESS_RING_BUFFER_CAPACITY, sizeof(packet_player_keypress_t));

    player_self_ref = out_player_self;
}

void player_remote_create(packet_player_add_t *packet, player_remote_t *out_player_remote)
{
    ASSERT(out_player_remote);

    memset(out_player_remote, 0, sizeof(player_remote_t));
    player_base_create(packet->id, packet->name, packet->position, packet->color, packet->health,
                       packet->state, packet->direction, &out_player_remote->base);

    out_player_remote->last_position = packet->position;
    out_player_remote->is_interpolated = false;
}

void player_self_destroy(player_self_t *player)
{
    ring_buffer_destroy(player->keypress_ring_buffer);
}

void player_self_update(player_self_t *player, f64 delta_time)
{
    if (player->base.state == PLAYER_STATE_DEAD) {
        return;
    }

    static f32 attack_cooldown_accumulator = 0.0f;
    static b8 attack_ready = true;
    if (!attack_ready) {
        attack_cooldown_accumulator += delta_time;
    }
    if (attack_cooldown_accumulator >= PLAYER_ATTACK_COOLDOWN) {
        attack_ready = true;
    }

    u32 key = 0;
    u32 mods = 0;
    vec2 movement = vec2_zero();

    if (player_keys_state[KEYCODE_Space] && attack_ready) {
        key = KEYCODE_Space;
        player->base.state = PLAYER_STATE_ATTACK;
        attack_ready = false;
        attack_cooldown_accumulator = 0.0f;
    } else if (player_keys_state[KEYCODE_W]) {
        key = KEYCODE_W;
        movement.y += PLAYER_VELOCITY;
        player->base.direction = PLAYER_DIRECTION_UP;
        if (player_keys_state[KEYCODE_LeftShift]) {
            mods = KEYMOD_SHIFT;
            player->base.state = PLAYER_STATE_ROLL;
            movement.y += PLAYER_VELOCITY;
        } else {
            player->base.state = PLAYER_STATE_WALK;
        }
    } else if (player_keys_state[KEYCODE_S]) {
        key = KEYCODE_S;
        movement.y -= PLAYER_VELOCITY;
        player->base.direction = PLAYER_DIRECTION_DOWN;
        if (player_keys_state[KEYCODE_LeftShift]) {
            mods = KEYMOD_SHIFT;
            player->base.state = PLAYER_STATE_ROLL;
            movement.y -= PLAYER_VELOCITY;
        } else {
            player->base.state = PLAYER_STATE_WALK;
        }
    } else if (player_keys_state[KEYCODE_A]) {
        key = KEYCODE_A;
        movement.x -= PLAYER_VELOCITY;
        player->base.direction = PLAYER_DIRECTION_LEFT;
        if (player_keys_state[KEYCODE_LeftShift]) {
            mods = KEYMOD_SHIFT;
            player->base.state = PLAYER_STATE_ROLL;
            movement.x -= PLAYER_VELOCITY;
        } else {
            player->base.state = PLAYER_STATE_WALK;
        }
    } else if (player_keys_state[KEYCODE_D]) {
        key = KEYCODE_D;
        movement.x += PLAYER_VELOCITY;
        player->base.direction = PLAYER_DIRECTION_RIGHT;
        if (player_keys_state[KEYCODE_LeftShift]) {
            mods = KEYMOD_SHIFT;
            player->base.state = PLAYER_STATE_ROLL;
            movement.x += PLAYER_VELOCITY;
        } else {
            player->base.state = PLAYER_STATE_WALK;
        }
    } else {
        if (player->base.state != PLAYER_STATE_DEAD) {
            player->base.state = PLAYER_STATE_IDLE;
            return;
        } else {
            return;
        }
    }

    player->base.position = vec2_add(player->base.position, movement);

    vec2 window_size = window_get_size();
    if (is_camera_locked_on_player) {
        camera_set_position(&game_camera, vec2_sub(player->base.position, vec2_divide(window_size, 2)));
    }

    packet_player_keypress_t player_keypress_packet = {
        .id = player->base.id,
        .seq_nr = packet_get_next_sequence_number(),
        .key = key,
        .mods = mods,
        .action = INPUTACTION_Press
    };

    b8 enqueue_status;
    ring_buffer_enqueue(player->keypress_ring_buffer, player_keypress_packet, &enqueue_status);
    if (!enqueue_status) {
        LOG_ERROR("failed to enqueue player keypress packet");
        return;
    }

    if (!packet_send(client_socket, PACKET_TYPE_PLAYER_KEYPRESS, &player_keypress_packet)) {
        LOG_ERROR("failed to send player keypress packet");
    }
}

void player_self_handle_authoritative_update(player_self_t *player, packet_player_update_t *packet)
{
    vec2 window_size = window_get_size();
    for (;;) {
        b8 dequeue_status;
        packet_player_keypress_t keypress = {0};

        ring_buffer_dequeue(player->keypress_ring_buffer, &keypress, &dequeue_status);
        if (!dequeue_status) {
            LOG_WARN("ran out of keypresses and did not find appropriate sequence number");
            break;
        }

        if (keypress.seq_nr == packet->seq_nr) {
            // Got authoritative position update from the server, so update self_player position received from the server
            // and re-apply all keypresses that happened since then
            player->base.position = packet->position;

            u32 nth_element = 0;
            while (ring_buffer_peek_from_end(player->keypress_ring_buffer, nth_element++, &keypress)) {
                i32 multiplier = (keypress.mods & KEYMOD_SHIFT) ? 2 : 1;
                if (keypress.key == KEYCODE_W) {
                    player->base.position.y += PLAYER_VELOCITY * multiplier;
                } else if (keypress.key == KEYCODE_S) {
                    player->base.position.y -= PLAYER_VELOCITY * multiplier;
                } else if (keypress.key == KEYCODE_A) {
                    player->base.position.x -= PLAYER_VELOCITY * multiplier;
                } else if (keypress.key == KEYCODE_D) {
                    player->base.position.x += PLAYER_VELOCITY * multiplier;
                }
            }
            if (is_camera_locked_on_player) {
                camera_set_position(&game_camera, vec2_sub(player->base.position, vec2_divide(window_size, 2)));
            }
            break;
        }
    }
}

void player_remote_handle_authoritative_update(player_remote_t *player, packet_player_update_t *packet)
{
    player->last_position = player->base.position;
    player->is_interpolated = true;
    player->no_update_accumulator = 0.0f;
    player->base.position = packet->position;
    player->base.state = packet->state;
    player->base.direction = packet->direction;
}

void player_take_damage(player_base_t *player, u32 damage)
{
    player->health -= damage;
    player->animation.damage.is_damaged = true;
    if (player->health <= 0) {
        player->state = PLAYER_STATE_DEAD;
        player->animation.player.keyframe_index = 0;
        player->animation.player.accumulator = 0.0f;
    }
}

static void player_base_render(player_base_t *base, f64 delta_time, vec2 position)
{
    vec2 (*animation_tex_coord)[4];
    if (base->state == PLAYER_STATE_IDLE) {
        animation_tex_coord = &idle_tex_coord[base->direction][base->animation.player.keyframe_index];
    } else if (base->state == PLAYER_STATE_WALK) {
        animation_tex_coord = &walk_tex_coord[base->direction][base->animation.player.keyframe_index];
    } else if (base->state == PLAYER_STATE_ATTACK) {
        animation_tex_coord = &attack_tex_coord[base->direction][base->animation.player.keyframe_index];
    } else if (base->state == PLAYER_STATE_ROLL) {
        animation_tex_coord = &roll_tex_coord[base->direction][base->animation.player.keyframe_index];
    } else if (base->state == PLAYER_STATE_BLOCK) {
        animation_tex_coord = &block_tex_coord[base->direction];
    } else if (base->state == PLAYER_STATE_DEAD) {
        animation_tex_coord = &dead_tex_coord[base->animation.player.keyframe_index];
    } else {
        LOG_ERROR("unknown base player state: %d", base->state);
        return;
    }

    vec2 tex_coord[4] = {0};
    memcpy(tex_coord, animation_tex_coord, sizeof(vec2) * 4);

    if (base->animation.damage.is_damaged && base->animation.damage.accumulator >= PLAYER_ATTACK_COOLDOWN) {
        base->animation.damage.is_damaged = false;
        base->animation.damage.accumulator = 0.0f;
    }

    vec3 color = vec3_create(1.0f, 1.0f, 1.0f);
    if (base->animation.damage.is_damaged) {
        color = COLOR_CRIMSON_RED;
        base->animation.damage.accumulator += delta_time;
    }

    renderer_draw_sprite_uv_color(&player_animation_spritesheet,
                                  tex_coord,
                                  vec2_create(KEYFRAME_WIDTH_PX, KEYFRAME_HEIGHT_PX),
                                  position,
                                  2.0f, 0.0f,
                                  color, 1.0f);
}

static void player_tick_animation(player_base_t *player, f64 delta_time)
{
    player->animation.player.accumulator += delta_time;
    if (player->animation.player.accumulator >= PLAYER_ANIMATION_FRAME_DURATION) {
        if (player->state == PLAYER_STATE_DEAD) {
            player->animation.player.keyframe_index = PLAYER_ANIMATION_KEYFRAME_COUNT - 1;
            player->animation.player.accumulator = 0.0f;
        } else {
            player->animation.player.keyframe_index = (player->animation.player.keyframe_index + 1) % PLAYER_ANIMATION_KEYFRAME_COUNT;
            player->animation.player.accumulator = 0.0f;
        }
    }
}

void player_self_render(player_self_t *player, f64 delta_time)
{
    player_tick_animation(&player->base, delta_time);
    player_base_render(&player->base, delta_time, player->base.position);

    vec2 username_position = vec2_create(
        player->base.position.x - (renderer_get_font_width(FA16) * strlen(player->base.name))/2,
        player->base.position.y + KEYFRAME_HEIGHT_PX/2 + 7.0f
    );
    renderer_draw_text(player->base.name, FA16, username_position, 1.0f, COLOR_MILK, 1.0f);
}

void player_remote_render(player_remote_t *player, f64 delta_time, f32 server_update_accumulator)
{
    player_tick_animation(&player->base, delta_time);

    // Check if there were no updates recently, and if so, reset the animation to idle
    player->no_update_accumulator += delta_time;
    if (player->base.state != PLAYER_STATE_DEAD ) {
        f32 cooldown = player->base.state == PLAYER_STATE_ATTACK ? (PLAYER_ATTACK_COOLDOWN * 3.0f) : PLAYER_ANIMATION_FRAME_DURATION;
        if (player->no_update_accumulator >= cooldown) {
            player->no_update_accumulator = 0.0f;
            player->base.state = PLAYER_STATE_IDLE;
            player_reset_player_animation(&player->base);
        }
    }

    vec2 position;
    if (player->is_interpolated) {
        // Interpolate player's position based on the current and last position and time since last server update
        f32 t = server_update_accumulator * SERVER_TICK_RATE;
        if (t > 1.0f) {
            t = 1.0f;
            player->is_interpolated = false;
        }

        vec2 player_position = {
            .x = math_lerpf(player->last_position.x, player->base.position.x, t),
            .y = math_lerpf(player->last_position.y, player->base.position.y, t)
        };

        position = player_position;
    } else {
        position = player->base.position;
    }

    player_base_render(&player->base, delta_time, position);

    char buffer[PLAYER_MAX_NAME_LENGTH + 32] = {0};
    snprintf(buffer, sizeof(buffer), "%s (%d)", player->base.name, player->base.health);

    vec2 username_position = vec2_create(
        position.x - (renderer_get_font_width(FA16) * strlen(buffer))/2,
        position.y + KEYFRAME_HEIGHT_PX/2 + 7.0f
    );
    renderer_draw_text(buffer, FA16, username_position, 1.0f, COLOR_MILK, 1.0f);
}

void player_respawn(player_base_t *player, packet_player_respawn_t *packet)
{
    if (player->state != PLAYER_STATE_DEAD) {
        LOG_WARN("tried to respawn player (id=%u) but not dead", player->id);
        return;
    }

    player->state = packet->state;
    player->health = packet->health;
    player->position = packet->position;
    player->direction = packet->direction;

    player->animation.player.keyframe_index = 0;
    player->animation.player.accumulator = 0.0f;
    player->animation.damage.is_damaged = false;
}

b8 player_key_pressed_event_callback(event_code_e code, event_data_t data)
{
    u16 key = data.u16[0];
    if (key == KEYCODE_W || key == KEYCODE_S || key == KEYCODE_A || key == KEYCODE_D || key == KEYCODE_Space || key == KEYCODE_LeftShift) {
        player_keys_state[key] = INPUTACTION_Press;
        if (player_self_ref->base.state != PLAYER_STATE_DEAD) {
            player_reset_player_animation(&player_self_ref->base);
        }
        return true;
    }

    return false;
}

b8 player_key_released_event_callback(event_code_e code, event_data_t data)
{
    u16 key = data.u16[0];
    if (key == KEYCODE_W || key == KEYCODE_S || key == KEYCODE_A || key == KEYCODE_D || key == KEYCODE_Space || key == KEYCODE_LeftShift) {
        player_keys_state[key] = INPUTACTION_Release;
        return true;
    }

    return false;
}
