#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "version.h"
#include "config.h"
#include "defines.h"
#include "renderer.h"
#include "event.h"
#include "chat.h"
#include "window.h"
#include "player.h"
#include "game_world.h"
#include "camera.h"
#include "color_palette.h"
#include "ui/ui.h"
#include "common/util.h"
#include "common/net.h"
#include "common/clock.h"
#include "common/asserts.h"
#include "common/strings.h"
#include "common/global.h"
#include "common/packet.h"
#include "common/logger.h"
#include "common/maths.h"
#include "common/input_codes.h"
#include "common/memory/memutils.h"
#include "common/containers/ring_buffer.h"

#define POLLFD_COUNT 2
#define INPUT_BUFFER_SIZE 4096
#define OVERFLOW_BUFFER_SIZE 1024
#define POLL_INFINITE_TIMEOUT -1

extern vec2 main_window_size;

static u32 remote_player_count = 0;
static player_remote_t remote_players[MAX_PLAYER_COUNT];
static player_self_t self_player;

static f32 server_update_accumulator = 0.0f;

static struct pollfd pfds[POLLFD_COUNT];

static char input_buffer[INPUT_BUFFER_SIZE] = {0};
static u32 input_count = 0;

static b8 running = false;
static b8 connected = false;
static b8 just_disconnected = false;

static vec2 mouse_position;

static b8 game_world_initialized = false;
static game_world_t game_world;

static pthread_t network_thread;

// Renderer stats are displayed on the UI, which means that in order to display
// all the information (including UI renderer calls) it needs to show previous frame's stats
static renderer_stats_t prev_frame_renderer_stats;

static b8   client_attempt_connection(const char *ip, const char *port);
static void client_init_game_resources(void);
static void client_shutdown_game_resources(void);

#if defined(DEBUG)
static b8 ui_debug_info_visible = true;
static b8 ui_test_panel_visible = false;
#endif

// Data referenced from somewhere else
char username[PLAYER_MAX_NAME_LENGTH];
i32 client_socket;
b8 is_camera_locked_on_player = true;
camera_t ui_camera;
camera_t game_camera;

static b8 handle_client_validation(i32 client)
{
    u64 puzzle_buffer;
    i64 bytes_read, bytes_sent;

    bytes_read = net_recv(client, (void *)&puzzle_buffer, sizeof(puzzle_buffer), 0); /* TODO: Handle unresponsive server */
    if (bytes_read <= 0) {
        if (bytes_read == -1) {
            LOG_ERROR("validation: recv error: %s", strerror(errno));
        } else if (bytes_read == 0) {
            LOG_ERROR("validation: orderly shutdown");
        }
        return false;
    }

    if (bytes_read == sizeof(puzzle_buffer)) {
        u64 answer = puzzle_buffer ^ 0xDEADBEEFCAFEBABE; /* TODO: Come up with a better validation function */
        bytes_sent = net_send(client, (void *)&answer, sizeof(answer), 0);
        if (bytes_sent == -1) {
            LOG_ERROR("validation: send error: %s", strerror(errno));
            return false;
        } else if (bytes_sent != sizeof(answer)) {
            LOG_ERROR("validation: failed to send %lu bytes of validation data", sizeof(answer));
            return false;
        }

        b8 status_buffer;
        bytes_read = net_recv(client, (void *)&status_buffer, sizeof(status_buffer), 0);
        if (bytes_read <= 0) {
            if (bytes_read == -1) {
                LOG_ERROR("validation status: recv error: %s", strerror(errno));
            } else if (bytes_read == 0) {
                LOG_ERROR("validation status: orderly shutdown");
            }
            return false;
        }

        return status_buffer;
    }

    LOG_ERROR("validation: received incorrect number of bytes");
    return false;
}

static void send_ping_packet(void)
{
    u64 time_now = clock_get_absolute_time_ns();

    packet_ping_t ping_packet = {
        .time = time_now
    };
    if (!packet_send(client_socket, PACKET_TYPE_PING, &ping_packet)) {
        LOG_ERROR("failed to send ping packet");
    }
}

static void handle_stdin_event(void)
{
    char ch;
    if (read(STDIN_FILENO, &ch, 1) == -1) {
        LOG_ERROR("stdin read: %s", strerror(errno));
        return;
    }

    if (ch >= 32 && ch < 127) { /* Printable characters */
        input_buffer[input_count++] = ch;
    } else if (ch == '\n') { /* Send packet over the socket based on input buffer */
        if (input_buffer[0] == '/') { /* Handle special commands */
            if (strcmp(&input_buffer[1], "ping") == 0) {
                send_ping_packet();
            } else if (strcmp(&input_buffer[1], "quit") == 0) {
                close(client_socket);
                exit(EXIT_SUCCESS);
            } else {
                LOG_WARN("unknown command");
            }

            input_count = 0;
            mem_zero(input_buffer, INPUT_BUFFER_SIZE);
        } else if (input_count > 0) {
            char *input_trimmed = string_trim(input_buffer);
            if (strlen(input_trimmed) < 1) {
                return;
            }

            packet_message_t message_packet = {0};
            message_packet.type = MESSAGE_TYPE_PLAYER;
            u32 username_size = strlen(username) > PLAYER_MAX_NAME_LENGTH ? PLAYER_MAX_NAME_LENGTH : strlen(username);
            strncpy(message_packet.author, username, username_size);
            u32 content_size = input_count > MESSAGE_MAX_CONTENT_LENGTH ? MESSAGE_MAX_CONTENT_LENGTH : input_count;
            strncpy(message_packet.content, input_trimmed, content_size);

            if (!packet_send(client_socket, PACKET_TYPE_MESSAGE, &message_packet)) {
                LOG_ERROR("failed to send message packet");
            }

            input_count = 0;
            mem_zero(input_buffer, INPUT_BUFFER_SIZE);
        }
    }
}

static void handle_socket_event(void)
{
    u8 recv_buffer[INPUT_BUFFER_SIZE + OVERFLOW_BUFFER_SIZE] = {0};

    i64 bytes_read = net_recv(client_socket, recv_buffer, INPUT_BUFFER_SIZE, 0);
    if (bytes_read <= 0) {
        if (bytes_read == -1) {
            LOG_ERROR("recv error: %s", strerror(errno));
        } else if (bytes_read == 0) {
            LOG_INFO("orderly shutdown: disconnected from server");
            close(client_socket);
            exit(EXIT_FAILURE);
        }
        return;
    }

    ASSERT_MSG(bytes_read >= PACKET_TYPE_SIZE[PACKET_TYPE_HEADER], "unimplemented: at least header size amount of bytes must be read");

    // Check if multiple packets included in single tcp data reception
    u8 *buffer = recv_buffer;
    for (;;) {
        packet_header_t *header = (packet_header_t *)buffer;

        if (bytes_read - PACKET_TYPE_SIZE[PACKET_TYPE_HEADER] < header->size) {
            u64 missing_bytes = header->size - (bytes_read - PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);
            LOG_TRACE("not read the entire packet body. reading %lu more", missing_bytes);

            ASSERT_MSG(missing_bytes <= OVERFLOW_BUFFER_SIZE, "not enough space in overflow buffer. consider increasing the size");

            // Read into OVERFLOW_BUFFER of the recv_buffer and proceed to packet interpretation
            i64 new_bytes_read = net_recv(client_socket, &recv_buffer[INPUT_BUFFER_SIZE], missing_bytes, 0);
            UNUSED(new_bytes_read); // prevents compiler warning in release mode
            ASSERT(new_bytes_read == missing_bytes);
        }

        u32 received_data_size = 0;
        switch (header->type) {
            case PACKET_TYPE_NONE: {
                LOG_WARN("received PACKET_TYPE_NONE, ignoring...");
            } break;
            case PACKET_TYPE_PING: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PING];
                packet_ping_t *data = (packet_ping_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);

                u64 time_now = clock_get_absolute_time_ns();
                f64 ping_ms = (time_now - data->time) / 1000000.0;
                LOG_TRACE("ping = %fms", ping_ms);
            } break;
            case PACKET_TYPE_MESSAGE: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_MESSAGE];
                packet_message_t *message = (packet_message_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);
                struct tm *local_time = localtime((time_t *)&message->timestamp);
                LOG_TRACE("[%d-%02d-%02d %02d:%02d] %s: %s",
                    local_time->tm_year + 1900,
                    local_time->tm_mon + 1,
                    local_time->tm_mday,
                    local_time->tm_hour,
                    local_time->tm_min,
                    message->author,
                    message->content);

                if (message->type == MESSAGE_TYPE_SYSTEM) {
                    chat_add_system_message(message->content);
                } else if (message->type == MESSAGE_TYPE_PLAYER) {
                    chat_player_message_t msg = {0};
                    mem_copy(msg.name, message->author, strlen(message->author));
                    mem_copy(msg.content, message->content, strlen(message->content));
                    chat_add_player_message(msg);
                } else {
                    LOG_ERROR("unknown single message type %d", message->type);
                }
            } break;
            case PACKET_TYPE_MESSAGE_HISTORY: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_MESSAGE_HISTORY];
                packet_message_history_t *message_history = (packet_message_history_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);
                for (u32 i = 0; i < message_history->count; i++) {
                    packet_message_t message = message_history->history[i];
                    if (message.type == MESSAGE_TYPE_SYSTEM) {
                        chat_add_system_message(message.content);
                    } else if (message.type == MESSAGE_TYPE_PLAYER) {
                        chat_player_message_t msg = {0};
                        mem_copy(msg.name, message.author, strlen(message.author));
                        mem_copy(msg.content, message.content, strlen(message.content));
                        chat_add_player_message(msg);
                    } else {
                        LOG_ERROR("unknown history message type %d", message.type);
                    }
                }
            } break;
            case PACKET_TYPE_PLAYER_INIT: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_INIT];
                packet_player_init_t *player_init = (packet_player_init_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);
                player_self_create(username, player_init, &self_player);
                LOG_INFO("initialized self: id=%u position=(%f,%f) color=(%f,%f,%f)",
                        self_player.base.id,
                        self_player.base.position.x, self_player.base.position.y,
                        self_player.base.color.r, self_player.base.color.g, self_player.base.color.b);

                packet_player_init_confirm_t player_confirm_packet = {0};
                player_confirm_packet.id = self_player.base.id;
                mem_copy(player_confirm_packet.name, username, strlen(username));
                if (!packet_send(client_socket, PACKET_TYPE_PLAYER_INIT_CONF, &player_confirm_packet)) {
                    LOG_ERROR("failed to send player init confirm packet");
                }

                camera_set_position(&game_camera, self_player.base.position);
            } break;
            case PACKET_TYPE_PLAYER_ADD: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_ADD];
                packet_player_add_t *player_add = (packet_player_add_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);

                b8 found_free_slot = false;
                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (remote_players[i].base.id == PLAYER_INVALID_ID) { /* Free slot */
                        LOG_INFO("adding new remote player id=%u", player_add->id);
                        player_remote_create(player_add, &remote_players[i]);
                        remote_player_count++;
                        found_free_slot = true;
                        break;
                    }
                }
                if (!found_free_slot) {
                    LOG_ERROR("failed to add new remote player, no free slots - remote_player_count=%u", remote_player_count);
                }
            } break;
            case PACKET_TYPE_PLAYER_REMOVE: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_REMOVE];
                packet_player_remove_t *player_remove = (packet_player_remove_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);
                b8 found_player_to_remove = false;
                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (remote_players[i].base.id == player_remove->id) {
                        remote_players[i].base.id = PLAYER_INVALID_ID;
                        found_player_to_remove = true;
                        break;
                    }
                }
                if (!found_player_to_remove) {
                    LOG_ERROR("failed to find player to remove with id=%u", player_remove->id);
                } else {
                    LOG_INFO("removed player with id=%d", player_remove->id);
                }
            } break;
            case PACKET_TYPE_PLAYER_UPDATE: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_UPDATE];
                packet_player_update_t *player_update = (packet_player_update_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);

                if (player_update->id == self_player.base.id) {
                    player_self_handle_authoritative_update(&self_player, player_update);
                    break;
                }

                b8 found_player_to_update = false;
                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (remote_players[i].base.id == player_update->id) {
                        player_remote_handle_authoritative_update(&remote_players[i], player_update);
                        found_player_to_update = true;
                        server_update_accumulator = 0.0f;
                        break;
                    }
                }
                if (!found_player_to_update) {
                    LOG_ERROR("failed to update player with id=%u", player_update->id);
                }
            } break;
            case PACKET_TYPE_PLAYER_HEALTH: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_HEALTH];
                packet_player_health_t *player_health_packet = (packet_player_health_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);

                if (player_health_packet->id == self_player.base.id) {
                    player_take_damage(&self_player.base, player_health_packet->damage);
                    break;
                }

                b8 found_player_to_update = false;
                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (remote_players[i].base.id == player_health_packet->id) {
                        player_take_damage(&remote_players[i].base, player_health_packet->damage);
                        found_player_to_update = true;
                        break;
                    }
                }
                if (!found_player_to_update) {
                    LOG_ERROR("failed to update health of player with id=%u", player_health_packet->id);
                }
            } break;
            case PACKET_TYPE_PLAYER_DEATH: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_DEATH];
                packet_player_death_t *player_death_packet = (packet_player_death_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);

                if (player_death_packet->id == self_player.base.id) {
                    if (self_player.base.state != PLAYER_STATE_DEAD) {
                        self_player.base.state = PLAYER_STATE_DEAD;
                        self_player.base.animation.player.keyframe_index = 0;
                        self_player.base.animation.player.accumulator = 0.0f;
                    }
                    break;
                }

                b8 found_player_to_update = false;
                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (remote_players[i].base.id == player_death_packet->id) {
                        if (remote_players[i].base.state != PLAYER_STATE_DEAD) {
                            remote_players[i].base.state = PLAYER_STATE_DEAD;
                            remote_players[i].base.animation.player.keyframe_index = 0;
                            remote_players[i].base.animation.player.accumulator = 0.0f;
                        }
                        found_player_to_update = true;
                        break;
                    }
                }
                if (!found_player_to_update) {
                    LOG_ERROR("failed to update death of player with id=%u", player_death_packet->id);
                }
            } break;
            case PACKET_TYPE_PLAYER_RESPAWN: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_RESPAWN];
                packet_player_respawn_t *player_respawn_packet = (packet_player_respawn_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);

                if (player_respawn_packet->id == self_player.base.id) {
                    player_respawn(&self_player.base, player_respawn_packet);
                    break;
                }

                b8 found_player_to_update = false;
                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (remote_players[i].base.id == player_respawn_packet->id) {
                        player_respawn(&remote_players[i].base, player_respawn_packet);
                        found_player_to_update = true;
                        break;
                    }
                }
                if (!found_player_to_update) {
                    LOG_ERROR("failed to update respawn of player with id=%u", player_respawn_packet->id);
                }
            } break;
            case PACKET_TYPE_GAME_WORLD_INIT: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_GAME_WORLD_INIT];
                packet_game_world_init_t *game_world_init_packet = (packet_game_world_init_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);

                LOG_TRACE("game world init packet received: seed=%u, octave_count=%i, bias=%.2f",
                          game_world_init_packet->map.seed,
                          game_world_init_packet->map.octave_count,
                          game_world_init_packet->map.bias);

                game_world_init(game_world_init_packet, &game_world);
                event_system_fire(EVENT_CODE_GAME_WORLD_INIT, (event_data_t){0});
            } break;
            case PACKET_TYPE_GAME_WORLD_OBJECT_ADD: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_GAME_WORLD_OBJECT_ADD];
                packet_world_object_add_t *world_object_add_packet = (packet_world_object_add_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);

                game_world_add_objects(&game_world, world_object_add_packet->objects, world_object_add_packet->length);
            } break;
            default:
                LOG_WARN("received unknown packet type, ignoring...");
        }

        u64 parsed_packet_size = PACKET_TYPE_SIZE[PACKET_TYPE_HEADER] + received_data_size;
        buffer = (buffer + parsed_packet_size);
        bytes_read -= parsed_packet_size;
        packet_header_t *next_header = (packet_header_t *)buffer;
        if (next_header->type <= PACKET_TYPE_NONE || next_header->type >= PACKET_TYPE_COUNT) {
            break;
        }
    }
}

static void *handle_networking(void *args)
{
    while (connected) {
        i32 num_events = poll(pfds, POLLFD_COUNT, POLL_INFINITE_TIMEOUT);

        if (num_events == -1) {
            if (errno == EINTR) {
                LOG_TRACE("interrupted 'poll' system call");
                break;
            }
            LOG_ERROR("poll error: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        for (i32 i = 0; i < POLLFD_COUNT; i++) {
            if (pfds[i].revents & POLLIN) {
                if (pfds[i].fd == STDIN_FILENO) { /* Input from stdin */
                    handle_stdin_event();
                } else if (pfds[i].fd == client_socket) { /* Server trying to send data */
                    handle_socket_event();
                }
            }
        }
    }

    if (close(client_socket) == -1) {
        LOG_ERROR("error while closing the socket: %s", strerror(errno));
    } else {
        LOG_INFO("closed client socket");
    }

    return NULL;
}

static b8 key_pressed_event_callback(event_code_e code, event_data_t data)
{
    u16 key = data.u16[0];
    if (game_world_initialized) {
        if (key == KEYCODE_P) {
            send_ping_packet();
            return true;
        } else if (key == KEYCODE_Escape) {
            window_set_cursor_state(false);
            return true;
        } else if (key == KEYCODE_Y) {
            is_camera_locked_on_player = !is_camera_locked_on_player;
            if (is_camera_locked_on_player) {
                camera_set_position(&game_camera, self_player.base.position);
            }
            return true;
        }
#if defined(DEBUG)
        else if (key == KEYCODE_F1) {
            ui_debug_info_visible = !ui_debug_info_visible;
            return true;
        } else if (key == KEYCODE_F2) {
            ui_test_panel_visible = !ui_test_panel_visible;
            return true;
        }
#endif
    } else {
#if defined(DEBUG)
        if (key == KEYCODE_C) {
            static char *debug_ip = "127.0.0.1";
            static char *debug_port = "8888";
            if (client_attempt_connection(debug_ip, debug_port)) {
                connected = true;
                client_init_game_resources();
                pthread_create(&network_thread, NULL, handle_networking, NULL);
            } else {
                LOG_ERROR("attempt at connecting to %s:%s failed", debug_ip, debug_port);
            }
            return true;
        }
#endif
    }

    return false;
}

static b8 mouse_button_pressed_event_callback(event_code_e code, event_data_t data)
{
    if (!window_is_cursor_captured()) {
        window_set_cursor_state(true);
        return true;
    }

    return false;
}

static b8 mouse_moved_event_callback(event_code_e code, event_data_t data)
{
    f32 xpos = data.f32[0];
    f32 ypos = data.f32[1];

    mouse_position.x = xpos;
    mouse_position.y = ypos;

    return true;
}

static b8 mouse_scrolled_event_callback(event_code_e code, event_data_t data)
{
    f32 y_offset = data.f32[1];
    camera_zoom(&game_camera, y_offset / 10.0f);
    return true;
}

static b8 window_closed_event_callback(event_code_e code, event_data_t data)
{
    running = false;
    return true;
}

static b8 window_resized_event_callback(event_code_e code, event_data_t data)
{
    camera_recalculate_projection(&ui_camera);

    if (game_world_initialized) {
        camera_recalculate_projection(&game_camera);

        if (is_camera_locked_on_player) {
            camera_set_position(&game_camera, self_player.base.position);
        }
    }

    return false;
}

// Event fired after receiving GAME_WORLD_INIT packet
// Made as event callback so that resources are created in the main thread with OpenGL context
static b8 game_world_init_callback(event_code_e code, event_data_t data)
{
    game_world_load_resources(&game_world);
    game_world_initialized = true;
    LOG_TRACE("game world resources loaded");
    return false;
}

static void signal_handler(i32 sig)
{
    if (sig == SIGINT) {
        running = false;
    }
}

static void display_build_version(void)
{
    f32 left = -main_window_size.x / 2.0f;
    f32 top  =  main_window_size.y / 2.0f;
    renderer_draw_text(BUILD_VERSION(CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR, CLIENT_VERSION_PATCH),
                       FA32,
                       vec2_create(left + 3.0f, top - renderer_get_font_height(FA32)),
                       1.0f,
                       COLOR_MILK,
                       1.0f);
}

#if defined(DEBUG)
static void display_debug_info(f64 delta_time)
{
    static ui_window_config_t debug_window_conf = {
        .position  = (vec2){ .x =   5, .y =  30 },
        .size      = (vec2){ .x = 200, .y = 440 },
        .draggable = true,
        .resizable = false,
        .font_size = FA16,
    };

    ui_begin_conf("debug window", &debug_window_conf, &ui_debug_info_visible);

    ui_text("INFO");

    char buffer[256] = {0};

    static f32 update_fps_period = 0.5f;
    static f32 update_fps_accumulator = 0.0f;
    static f32 dt = 0.0f;

    update_fps_accumulator += delta_time;
    if (update_fps_accumulator >= update_fps_period) {
        dt = delta_time;
        update_fps_accumulator = 0.0f;
    }

    snprintf(buffer, sizeof(buffer), "fps: %d", (u32)(1.0f / dt));
    ui_text(buffer);

    snprintf(buffer, sizeof(buffer), "cursor captured: %s", window_is_cursor_captured() ? "true" : "false");
    ui_text(buffer);

    snprintf(buffer, sizeof(buffer), "camera\n  locked: %s\n  position\n    x: %.2f\n    y: %.2f",
             is_camera_locked_on_player ? "true" : "false", game_camera.position.x, game_camera.position.y);
    ui_text(buffer);

    static u64 network_up = 0.0f;
    static u64 network_down = 0.0f;
    net_get_bandwidth(&network_up, &network_down);

    f32 network_up_adjusted;
    f32 network_down_adjusted;
    const char *network_up_unit = get_size_unit(network_up, &network_up_adjusted);
    const char *network_down_unit = get_size_unit(network_down, &network_down_adjusted);
    snprintf(buffer, sizeof(buffer), "network\n  up: %0.2f %s/s\n  down: %0.2f %s/s",
             network_up_adjusted, network_up_unit, network_down_adjusted, network_down_unit);
    ui_text(buffer);

    char attack_buffer[32] = {0};
    if (self_player.base.attack_ready) {
        snprintf(attack_buffer, sizeof(attack_buffer), "ready");
    } else {
        snprintf(attack_buffer, sizeof(attack_buffer), "%0.2fs", PLAYER_ATTACK_COOLDOWN - self_player.base.attack_cooldown_accumulator);
    }

    char roll_buffer[32] = {0};
    if (self_player.base.roll_ready) {
        snprintf(roll_buffer, sizeof(roll_buffer), "ready");
    } else {
        snprintf(roll_buffer, sizeof(roll_buffer), "%0.2fs", PLAYER_ROLL_COOLDOWN - self_player.base.roll_cooldown_accumulator);
    }

    snprintf(buffer, sizeof(buffer), "cooldowns\n  attack: %s\n  roll: %s", attack_buffer, roll_buffer);
    ui_text(buffer);

    snprintf(buffer, sizeof(buffer), "renderer\n  quad count: %u\n  char count: %u\n  draw calls: %u",
             prev_frame_renderer_stats.quad_count, prev_frame_renderer_stats.char_count, prev_frame_renderer_stats.draw_calls);
    ui_text(buffer);

    if (game_world_initialized) {
        u64 chunks_count = game_world_get_chunk_num();
        u64 chunk_size = game_world_get_chunk_size();
        u64 chunk_mem_usage = chunks_count * chunk_size;
        f32 chunk_mem_usage_formatted;

        const char *unit = get_size_unit(chunk_mem_usage, &chunk_mem_usage_formatted);
        snprintf(buffer, sizeof(buffer), "chunks in cache\n  count: %llu\n  mem usage: %.02f %s", chunks_count, chunk_mem_usage_formatted, unit);
        ui_text(buffer);
    }

    char *usage_str = get_memory_usage_str();
    ui_text(usage_str);
    free(usage_str);

    ui_separator();
    ui_text("ACTIONS");
    static b8 line_mode = false;
    if (ui_checkbox("line mode", &line_mode)) {
        renderer_set_polygon_mode(line_mode ? POLYGON_MODE_LINE : POLYGON_MODE_FILL);
    }

    if (ui_button("disconnect client")) {
        connected = false;
        just_disconnected = true;
        game_world_initialized = false;
    }

    ui_end();
}

void display_ui_test_panel(void)
{
    static ui_window_config_t test_panel_window_conf = {
        .position  = { .x = 450.0f, .y = 50.0f  },
        .size      = { .x = 500.0f, .y = 700.0f },
        .draggable = true,
        .resizable = true,
        .font_size = FA32,
    };

    ui_begin_conf("ui test panel", &test_panel_window_conf, &ui_test_panel_visible);

    static b8 window_1_visible = false;
    static b8 window_2_visible = false;
    if (ui_button("show window_1")) {
        window_1_visible = !window_1_visible;
    }
    if (ui_button("show window_2")) {
        window_2_visible = !window_2_visible;
    }

    for (i32 i = 0; i < 3; i++) {
        char buf[32] = {0};
        snprintf(buf, sizeof(buf), "click %i", i);
        if (ui_button(buf)) {
            LOG_TRACE("clicked nr %i", i);
        }
    }

    static b8 world_visible = false;
    if (ui_button("toggle 'world'")) {
        world_visible = !world_visible;
    }
    ui_text("hello");
    if (world_visible) {
        ui_same_line();
        ui_text(" world");
        ui_same_line();
        ui_text("!");
        ui_text("this is below");
    }
    ui_text("noice");

    static i32 counter = 0;
    if (ui_button("+")) {
        counter++;
    }
    ui_same_line();
    if (ui_button("-")) {
        counter--;
    }
    ui_same_line();
    if (ui_button("r")) {
        counter = 0;
    }

    char buf[32] = {0};
    snprintf(buf, sizeof(buf), "%i", counter);
    ui_text("counter=");
    ui_same_line();
    ui_text(buf);

    ui_text(">>checkboxes");
    static b8 checkboxes[3] = { false, false, false };
    ui_checkbox("opt_1", &checkboxes[0]);
    ui_same_line();
    ui_checkbox("opt_2", &checkboxes[1]);
    ui_same_line();
    if (ui_checkbox("opt_3", &checkboxes[2])) {
        LOG_TRACE("opt_3 state changed");
    }
    ui_separator();

    ui_text(">>radio buttons");
    static i32 selected_radio = 0;
    ui_radiobutton("opt_4", &selected_radio, 0);
    ui_same_line();
    ui_radiobutton("opt_5", &selected_radio, 1);
    ui_same_line();
    if (ui_radiobutton("opt_6", &selected_radio, 2)) {
        LOG_TRACE("opt_6 state changed");
    }
    ui_separator();

    ui_text(">>slider");
    static f32 slider_value = 0.5f;
    ui_slider_float("label", &slider_value, -1.0f, 1.0f);
    ui_separator();

    ui_text(">>option carousel");
    static const char *carousel_options[] = { "binary", "hexadecimal", "octal", "decimal" };
    static u32 selected_option = 0;
    ui_opt_carousel("number representation", carousel_options, ARRAY_SIZE(carousel_options), &selected_option);

    ui_separator();
    ui_text(">>input box 1");
    static char input_text1[32] = "foo";
    if (ui_input_text("input_label_1", input_text1, ARRAY_SIZE(input_text1))) {
        LOG_TRACE("input box content: %s", input_text1);
    }

    ui_text(">>input box 2");
    static char input_text2[32] = "bar";
    ui_input_text("input_label_2", input_text2, ARRAY_SIZE(input_text2));

    ui_text("the end");

    ui_end();

    if (window_1_visible) {
        ui_begin("window_1", &window_1_visible);
        ui_text("hello, world!");
        ui_end();
    }
    if (window_2_visible) {
        ui_begin("window_2", &window_2_visible);
        ui_text("goodbye, world!");
        ui_end();
    }
}
#endif

static void check_camera_movement(f64 delta_time)
{
    if (!window_is_cursor_captured() || is_camera_locked_on_player) {
        return;
    }

    vec2 offset = vec2_zero();

    b8 camera_moved = false;
    if (mouse_position.x <= CAMERA_MOVE_BORDER_OFFSET) {
        offset.x -= 1;
        camera_moved = true;
    } else if (mouse_position.x >= main_window_size.x - CAMERA_MOVE_BORDER_OFFSET - 1) {
        offset.x += 1;
        camera_moved = true;
    }

    if (mouse_position.y <= CAMERA_MOVE_BORDER_OFFSET) {
        offset.y += 1;
        camera_moved = true;
    } else if (mouse_position.y >= main_window_size.y - CAMERA_MOVE_BORDER_OFFSET - 1) {
        offset.y -= 1;
        camera_moved = true;
    }

    if (camera_moved) {
        f32 velocity = CAMERA_MOVE_SPEED_PPS * delta_time;
        vec2 normalized = vec2_normalize(offset);
        vec2 velocity_vec = vec2_mul(normalized, velocity);
        camera_move(&game_camera, velocity_vec);
    }
}

static b8 client_attempt_connection(const char *ip, const char *port)
{
    struct addrinfo hints, *result, *rp;
    mem_zero(&hints, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    i32 status_code = getaddrinfo(ip, port, &hints, &result);
    if (status_code != 0) {
        LOG_FATAL("getaddrinfo error: %s", gai_strerror(status_code));
        return false;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        client_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (client_socket == -1) {
            continue;
        }

        if (connect(client_socket, rp->ai_addr, rp->ai_addrlen) == -1) {
            // TODO: Provide more information about failed parameters
            LOG_ERROR("connect error: %s", strerror(errno));
            continue;
        }

        break;
    }

    freeaddrinfo(result);

    if (rp == NULL) {
        LOG_FATAL("failed to connect");
        return false;
    }

    LOG_INFO("connected to server at %s:%s", ip, port);

    if (!handle_client_validation(client_socket)) {
        LOG_FATAL("failed client validation");
        close(client_socket);
        return false;
    }

    LOG_INFO("client successfully validated");

    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;

    pfds[1].fd = client_socket;
    pfds[1].events = POLLIN;

    struct sigaction sa;
    sa.sa_flags = SA_RESTART; // Restart functions interruptable by EINTR like poll()
    sa.sa_handler = &signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);

    LOG_INFO("server tick rate: %u", SERVER_TICK_RATE);

    return true;
}

static void client_init_game_resources(void)
{
    event_system_register(EVENT_CODE_CHAR_PRESSED,         chat_char_pressed_event_callback);
    event_system_register(EVENT_CODE_KEY_PRESSED,          chat_key_pressed_event_callback);
    event_system_register(EVENT_CODE_KEY_REPEATED,         chat_key_repeated_event_callback);
    event_system_register(EVENT_CODE_MOUSE_BUTTON_PRESSED, chat_mouse_button_pressed_event_callback);
    event_system_register(EVENT_CODE_WINDOW_RESIZED,       chat_window_resized_event_callback);

    event_system_register(EVENT_CODE_KEY_PRESSED,  player_key_pressed_event_callback);
    event_system_register(EVENT_CODE_KEY_RELEASED, player_key_released_event_callback);

    event_system_register(EVENT_CODE_KEY_PRESSED,     game_world_key_pressed_event_callback);
    event_system_register(EVENT_CODE_GAME_WORLD_INIT, game_world_init_callback);

    event_system_register(EVENT_CODE_MOUSE_BUTTON_PRESSED, mouse_button_pressed_event_callback);
    event_system_register(EVENT_CODE_MOUSE_SCROLLED,       mouse_scrolled_event_callback);

    camera_create(&game_camera, vec2_zero());

    chat_init();
    player_load_animations();
}

static void client_shutdown_game_resources(void)
{
    LOG_INFO("client shutting down");

    // Tell server to remove ourselves from the player list
    packet_player_remove_t player_remove_packet = {
        .id = self_player.base.id
    };
    if (!packet_send(client_socket, PACKET_TYPE_PLAYER_REMOVE, &player_remove_packet)) {
        LOG_ERROR("failed to send player remove packet");
    }

    LOG_INFO("removed self from players");
    player_self_destroy(&self_player);

    event_system_unregister(EVENT_CODE_CHAR_PRESSED,         chat_char_pressed_event_callback);
    event_system_unregister(EVENT_CODE_KEY_PRESSED,          chat_key_pressed_event_callback);
    event_system_unregister(EVENT_CODE_KEY_REPEATED,         chat_key_repeated_event_callback);
    event_system_unregister(EVENT_CODE_MOUSE_BUTTON_PRESSED, chat_mouse_button_pressed_event_callback);
    event_system_unregister(EVENT_CODE_WINDOW_RESIZED,       chat_window_resized_event_callback);

    event_system_unregister(EVENT_CODE_KEY_PRESSED,  player_key_pressed_event_callback);
    event_system_unregister(EVENT_CODE_KEY_RELEASED, player_key_released_event_callback);

    event_system_unregister(EVENT_CODE_KEY_PRESSED,     game_world_key_pressed_event_callback);
    event_system_unregister(EVENT_CODE_GAME_WORLD_INIT, game_world_init_callback);

    event_system_unregister(EVENT_CODE_MOUSE_BUTTON_PRESSED, mouse_button_pressed_event_callback);
    event_system_unregister(EVENT_CODE_MOUSE_SCROLLED,       mouse_scrolled_event_callback);

    chat_shutdown();

    pthread_kill(network_thread, SIGUSR1);
    pthread_join(network_thread, NULL);
}

static void run_connected_client(f64 delta_time)
{
    static f64 client_update_accumulator = 0.0f;

    check_camera_movement(delta_time);

    server_update_accumulator += delta_time;
    client_update_accumulator += delta_time;

    if (client_update_accumulator >= CLIENT_TICK_DURATION) {
        player_self_update(&self_player, CLIENT_TICK_DURATION);
        client_update_accumulator = 0.0f;
    }

    renderer_reset_stats();
    renderer_clear_screen(vec4_create(0.3f, 0.3f, 0.3f, 1.0f));

    // Start of game rendering
    renderer_begin_scene(&game_camera);

    if (game_world_initialized) {
        game_world_render(&game_world, &game_camera);
    }

    // Render all other players
    for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (remote_players[i].base.id != PLAYER_INVALID_ID) {
            player_remote_render(&remote_players[i], delta_time, server_update_accumulator);
        }
    }

    // Render ourselves
    if (self_player.base.id != PLAYER_INVALID_ID) {
        player_self_render(&self_player, delta_time);
    }

    renderer_end_scene();

    // Start of UI rendering
    renderer_begin_scene(&ui_camera);

    chat_render();
    display_build_version();

    char health_buffer[16];
    snprintf(health_buffer, sizeof(health_buffer), "Health: %d", self_player.base.health);
    f32 corner_padding = 10.0f;
    vec2 health_position = vec2_create(
        main_window_size.x / 2.0f - (renderer_get_font_width(FA32) * strlen(health_buffer)) - corner_padding,
        main_window_size.y / 2.0f - renderer_get_font_height(FA32) - corner_padding
    );
    renderer_draw_text(health_buffer, FA32, health_position, 1.0f, COLOR_MILK, 1.0f);

    renderer_end_scene();

#if defined(DEBUG)
    ui_begin_frame();

    if (ui_debug_info_visible) {
        display_debug_info(delta_time);
    }
    if (ui_test_panel_visible) {
        display_ui_test_panel();
    }

    ui_end_frame();
#endif

    mem_copy(&prev_frame_renderer_stats, &renderer_stats, sizeof(renderer_stats));
}

static void run_disconnected_client(f64 delta_time)
{
    static vec2 ui_window_size = (vec2){ .x = 600, .y = 200 };
    renderer_clear_screen(vec4_create(0.8f, 0.5f, 0.3f, 1.0f));

    ui_begin_frame();
    {
        ui_window_config_t config = {
            .position = (vec2){
                .x = main_window_size.x * 0.5f - ui_window_size.x * 0.5f,
                .y = main_window_size.y * 0.5f - ui_window_size.y * 0.5f
            },
            .size = ui_window_size,
            .draggable = true,
            .resizable = true,
            .font_size = FA64
        };

        static b8 is_open = true;
        ui_begin_conf("connect to server", &config, &is_open);

        static char ip_addr_buf[15+1] = {0};
        ui_input_text("ip address", ip_addr_buf, ARRAY_SIZE(ip_addr_buf));

        static char port_buf[5+1] = {0};
        ui_input_text("      port", port_buf, ARRAY_SIZE(port_buf));

        if (ui_button("connect")) {
            if (client_attempt_connection(ip_addr_buf, port_buf)) {
                mem_zero(ip_addr_buf, ARRAY_SIZE(ip_addr_buf));
                mem_zero(port_buf, ARRAY_SIZE(port_buf));
                connected = true;
                client_init_game_resources();
                pthread_create(&network_thread, NULL, handle_networking, NULL);
            } else {
                LOG_ERROR("attempt at connecting to %s:%s failed", ip_addr_buf, port_buf);
            }
        }

        ui_end();
    }
    ui_end_frame();
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        LOG_FATAL("usage: %s username", argv[0]);
        exit(EXIT_FAILURE);
    }

    mem_copy(username, argv[1], strlen(argv[1]));

    if (!window_create(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "StarLore")) {
        LOG_ERROR("failed to create window");
        exit(EXIT_FAILURE);
    }

    if (!event_system_init()) {
        LOG_ERROR("failed to initialize event system");
        exit(EXIT_FAILURE);
    }

    if (!renderer_init()) {
        LOG_ERROR("failed to initialize renderer");
        exit(EXIT_FAILURE);
    }

    event_system_register(EVENT_CODE_MOUSE_BUTTON_PRESSED,  ui_mouse_button_pressed_event_callback);
    event_system_register(EVENT_CODE_MOUSE_BUTTON_RELEASED, ui_mouse_button_released_event_callback);
    event_system_register(EVENT_CODE_MOUSE_MOVED,           ui_mouse_moved_event_callback);
    event_system_register(EVENT_CODE_MOUSE_SCROLLED,        ui_mouse_scrolled_event_callback);
    event_system_register(EVENT_CODE_CHAR_PRESSED,          ui_char_pressed_event_callback);
    event_system_register(EVENT_CODE_KEY_PRESSED,           ui_key_pressed_event_callback);
    event_system_register(EVENT_CODE_KEY_REPEATED,          ui_key_repeated_event_callback);

    event_system_register(EVENT_CODE_KEY_PRESSED,    key_pressed_event_callback);
    event_system_register(EVENT_CODE_MOUSE_MOVED,    mouse_moved_event_callback);
    event_system_register(EVENT_CODE_WINDOW_CLOSED,  window_closed_event_callback);
    event_system_register(EVENT_CODE_WINDOW_RESIZED, window_resized_event_callback);

    camera_create(&ui_camera, vec2_zero());
    ui_init();

    f64 last_time = glfwGetTime();
    f64 delta_time = 0.0f;

    running = true;
    while (running) {
        f64 now = glfwGetTime();
        delta_time = now - last_time;
        last_time = now;
        net_update(delta_time);

        if (connected) {
            run_connected_client(delta_time);
        } else {
            run_disconnected_client(delta_time);
            if (just_disconnected) {
                client_shutdown_game_resources();
                just_disconnected = false;
            }
        }

        window_poll_events();
        window_swap_buffers();

        event_system_poll_events();
    }

    if (connected) {
        client_shutdown_game_resources();
    }

    event_system_unregister(EVENT_CODE_MOUSE_BUTTON_PRESSED,  ui_mouse_button_pressed_event_callback);
    event_system_unregister(EVENT_CODE_MOUSE_BUTTON_RELEASED, ui_mouse_button_released_event_callback);
    event_system_unregister(EVENT_CODE_MOUSE_MOVED,           ui_mouse_moved_event_callback);
    event_system_unregister(EVENT_CODE_MOUSE_SCROLLED,        ui_mouse_scrolled_event_callback);
    event_system_unregister(EVENT_CODE_CHAR_PRESSED,          ui_char_pressed_event_callback);
    event_system_unregister(EVENT_CODE_KEY_PRESSED,           ui_key_pressed_event_callback);
    event_system_unregister(EVENT_CODE_KEY_REPEATED,          ui_key_repeated_event_callback);

    event_system_unregister(EVENT_CODE_KEY_PRESSED,    key_pressed_event_callback);
    event_system_unregister(EVENT_CODE_MOUSE_MOVED,    mouse_moved_event_callback);
    event_system_unregister(EVENT_CODE_WINDOW_CLOSED,  window_closed_event_callback);
    event_system_unregister(EVENT_CODE_WINDOW_RESIZED, window_resized_event_callback);

    ui_shutdown();
    renderer_shutdown();
    event_system_shutdown();

    LOG_INFO("destroying main window");
    window_destroy();

    return EXIT_SUCCESS;
}
