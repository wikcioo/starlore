#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>

#include "defines.h"
#include "common/net.h"
#include "common/clock.h"
#include "common/player_types.h"
#include "common/global.h"
#include "common/packet.h"
#include "common/logger.h"
#include "common/asserts.h"
#include "common/maths.h"
#include "common/input_codes.h"
#include "common/perlin_noise.h"
#include "common/game_world_types.h"
#include "common/containers/darray.h"
#include "common/containers/ring_buffer.h"

#define INPUT_BUFFER_SIZE 4096
#define INPUT_OVERFLOW_BUFFER_SIZE 256

#define SERVER_BACKLOG 10
#define INPUT_RING_BUFFER_CAPACITY 256
#define PROCESSED_INPUT_LIMIT_PER_UPDATE 256

#define POLL_INFINITE_TIMEOUT -1

#define MAX_MESSAGE_LENGTH 1024

#define PLAYER_SPAWN_POSITION vec2_create(0, 0)

typedef struct {
    struct pollfd *fds;
    u32 count;
    u32 capacity;
} server_pfd_t;

typedef struct {
    i32 socket;
    player_id id;
    u32 seq_nr;
    char name[PLAYER_MAX_NAME_LENGTH];
    vec2 position;
    vec3 color;
    i32 health;
    player_direction_e direction;
    player_state_e state;
    f32 respawn_cooldown;
    f32 attack_cooldown;
    f32 attack_accumulator;
    f32 roll_start;
    f32 roll_cooldown;
    f32 roll_accumulator;
} player_t;

typedef struct {
    u32 type;
    i64 timestamp;
    char author[PLAYER_MAX_NAME_LENGTH];
    char content[MESSAGE_MAX_CONTENT_LENGTH];
} message_t;

static b8 running;
static i32 server_socket;
static server_pfd_t fds;
static player_t players[MAX_PLAYER_COUNT];
static player_id current_player_id = 1000;
static void *input_ring_buffer;
static message_t *messages;

static game_world_t game_world;

void server_pfd_init(u32 initial_capacity, server_pfd_t *out_server_pfd)
{
    out_server_pfd->count = 0;
    out_server_pfd->capacity = initial_capacity;

    u32 fds_size = sizeof(struct pollfd) * initial_capacity;
    out_server_pfd->fds = malloc(fds_size);
    memset(out_server_pfd->fds, 0, fds_size);
}

void server_pfd_shutdown(server_pfd_t *server_pfd)
{
    server_pfd->count = 0;
    server_pfd->capacity = 0;
    free(server_pfd->fds);
}

void server_pfd_add(server_pfd_t *pfd, i32 fd)
{
    if (pfd->count + 1 >= pfd->capacity) { /* Resize */
        void *ptr = realloc((void *)pfd->fds, pfd->capacity * 2);
        if (ptr != NULL) {
            pfd->capacity = pfd->capacity * 2;
            pfd->fds = ptr;
        } else {
            LOG_ERROR("failed to resize pollfd array");
        }
    }

    pfd->fds[pfd->count].fd = fd;
    pfd->fds[pfd->count].events = POLLIN;
    pfd->count++;
}

void server_pfd_remove(server_pfd_t *pfd, i32 fd)
{
    for (i32 i = 0; i < pfd->count; i++) {
        if (pfd->fds[i].fd == fd) {
            pfd->fds[i].fd = pfd->fds[pfd->count-1].fd; /* Replace old fd with the last one */
            pfd->count--;
            return;
        }
    }

    LOG_ERROR("did not find fd=%d in the array of pfds", fd);
}

void *get_in_addr(struct sockaddr *addr)
{
    if (addr->sa_family == AF_INET) {
        return &((struct sockaddr_in *)addr)->sin_addr;
    }

    return &((struct sockaddr_in6 *)addr)->sin6_addr;
}

static b8 receive_client_data(i32 client_socket, u8 *recv_buffer, u32 buffer_size, i64 *bytes_read);

b8 validate_incoming_client(i32 client_socket)
{
    u64 puzzle_value = clock_get_absolute_time_ns();

    i64 bytes_read, bytes_sent;

    bytes_sent = net_send(client_socket, (void *)&puzzle_value, sizeof(puzzle_value), 0);
    if (bytes_sent == -1) {
        LOG_ERROR("validation: send error: %s", strerror(errno));
        return false;
    } else if (bytes_sent != sizeof(puzzle_value)) {
        LOG_ERROR("validation: failed to send %lu bytes of validation data", sizeof(puzzle_value));
        return false;
    }

    u64 answer = puzzle_value ^ 0xDEADBEEFCAFEBABE; /* TODO: Come up with a better validation function */

    u64 result_buffer;
    bytes_read = net_recv(client_socket, (void *)&result_buffer, sizeof(result_buffer), 0);
    if (bytes_read <= 0) {
        if (bytes_read == -1) {
            LOG_ERROR("validation: recv error: %s", strerror(errno));
        } else if (bytes_read == 0) {
            LOG_ERROR("validation: orderly shutdown");
            /* Socket closed by the handler */
        }
        return false;
    }

    if (bytes_read == sizeof(result_buffer)) {
        b8 status_buffer = result_buffer == answer;
        bytes_sent = net_send(client_socket, (void *)&status_buffer, sizeof(status_buffer), 0);
        if (bytes_sent == -1) {
            LOG_ERROR("validation status: send error: %s", strerror(errno));
            return false;
        } else if (bytes_sent != sizeof(status_buffer)) {
            LOG_ERROR("validation status: failed to send %lu bytes of validation data", sizeof(puzzle_value));
            return false;
        }

        return status_buffer;
    }

    LOG_ERROR("validation: received incorrect number of bytes");
    return false;
}

void handle_new_player_connection(i32 client_socket)
{
    // Assign player to the new client
    f32 red   = math_frandom_range(0.0, 1.0);
    f32 green = math_frandom_range(0.0, 1.0);
    f32 blue  = math_frandom_range(0.0, 1.0);
    i32 new_player_idx;
    for (new_player_idx = 0; new_player_idx < MAX_PLAYER_COUNT; new_player_idx++) {
        if (players[new_player_idx].id == PLAYER_INVALID_ID) { /* Free slot */
            players[new_player_idx].socket   = client_socket;
            players[new_player_idx].id       = current_player_id;
            players[new_player_idx].position = PLAYER_SPAWN_POSITION;
            players[new_player_idx].color    = vec3_create(red, green, blue);
            players[new_player_idx].health   = PLAYER_START_HEALTH;
            break;
        }
    }

    packet_player_init_t player_init_packet = {
        .id        = current_player_id,
        .position  = PLAYER_SPAWN_POSITION,
        .color     = vec3_create(red, green, blue),
        .health    = PLAYER_START_HEALTH,
        .state     = PLAYER_STATE_IDLE,
        .direction = PLAYER_DIRECTION_DOWN
    };
    if (!packet_send(client_socket, PACKET_TYPE_PLAYER_INIT, &player_init_packet)) {
        LOG_ERROR("failed to send player init packet");
    }

    const u64 packet_size = PACKET_TYPE_SIZE[PACKET_TYPE_HEADER] + PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_INIT_CONF];
    u8 buffer[INPUT_BUFFER_SIZE] = {0};
    i64 bytes_read;
    if (!receive_client_data(client_socket, buffer, packet_size, &bytes_read)) {
        LOG_ERROR("failed to receive player init confirm packet from socket with fd=%d", client_socket);
        return;
    }

    if (bytes_read == packet_size) {
        packet_player_init_confirm_t *player_init_confirm_packet = (packet_player_init_confirm_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);
        if (player_init_confirm_packet->id != current_player_id) {
            LOG_ERROR("mismatched player init confirm id: expected=%d, actual=%d", current_player_id, player_init_confirm_packet->id);
            return;
        }

        memset(players[new_player_idx].name, 0, sizeof(players[new_player_idx].name));
        memcpy(players[new_player_idx].name, player_init_confirm_packet->name, strlen(player_init_confirm_packet->name));
    } else {
        LOG_ERROR("failed to read all required bytes for the player init confirm packet from socket with fd=%d", client_socket);
        return;
    }

    current_player_id++;

    // Send the rest of the players to the new player
    for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].id != PLAYER_INVALID_ID && players[i].id != player_init_packet.id) {
            packet_player_add_t player_add_packet = {
                .id        = players[i].id,
                .position  = players[i].position,
                .color     = players[i].color,
                .health    = players[i].health,
                .state     = players[i].state,
                .direction = players[i].direction
            };
            memcpy(player_add_packet.name, players[i].name, strlen(players[i].name));

            if (!packet_send(client_socket, PACKET_TYPE_PLAYER_ADD, &player_add_packet)) {
                LOG_ERROR("failed to send player add packet");
            }
        }
    }

    // Send messages history
    u64 messages_length = darray_length(messages);
    u32 counter = 0;
    packet_message_history_t message_history_packet = {0};
    for (u64 i = 0; i < messages_length; i++) {
        message_t message = messages[i];

        packet_message_t message_packet = {0};
        message_packet.type = message.type;
        if (message.author[0] != 0) {
            memcpy(message_packet.author, message.author, strlen(message.author));
        }
        memcpy(message_packet.content, message.content, strlen(message.content));

        memcpy(&message_history_packet.history[counter++], &message_packet, sizeof(message_packet));
        if (counter >= MAX_MESSAGE_HISTORY_LENGTH) {
            message_history_packet.count = counter;
            if (!packet_send(client_socket, PACKET_TYPE_MESSAGE_HISTORY, &message_history_packet)) {
                LOG_ERROR("failed to send %u messages in the message history packet", counter);
            }
            counter = 0;
        }
    }
    if (counter > 0) {
        message_history_packet.count = counter;
        if (!packet_send(client_socket, PACKET_TYPE_MESSAGE_HISTORY, &message_history_packet)) {
            LOG_ERROR("failed to send %u messages in the message history packet", counter);
        }
    }

    // Update other players with the new player and send system message to all
    packet_message_t message_packet = {0};
    message_packet.type = MESSAGE_TYPE_SYSTEM;
    snprintf(message_packet.content, sizeof(message_packet.content), "new player <%s> joined the game!", players[new_player_idx].name);

    for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].id != PLAYER_INVALID_ID) {
            if (players[i].id != player_init_packet.id) {
                packet_player_add_t player_add_packet = {
                    .id        = player_init_packet.id,
                    .position  = player_init_packet.position,
                    .color     = player_init_packet.color,
                    .health    = player_init_packet.health,
                    .state     = player_init_packet.state,
                    .direction = player_init_packet.direction
                };
                memcpy(player_add_packet.name, players[new_player_idx].name, strlen(players[new_player_idx].name));

                if (!packet_send(players[i].socket, PACKET_TYPE_PLAYER_ADD, &player_add_packet)) {
                    LOG_ERROR("failed to send player add packet");
                }
            }

            if (!packet_send(players[i].socket, PACKET_TYPE_MESSAGE, &message_packet)) {
                LOG_ERROR("failed to send message packet");
            }
        }
    }

    message_t msg = {0};
    msg.type = MESSAGE_TYPE_SYSTEM;
    memcpy(msg.content, message_packet.content, strlen(message_packet.content));
    darray_push(messages, msg);

    // Send game world initialization data to the new player
    packet_game_world_init_t world_init_packet = {0};
    memcpy(&world_init_packet.map, &game_world.map, sizeof(game_map_t));

    if (!packet_send(players[new_player_idx].socket, PACKET_TYPE_GAME_WORLD_INIT, &world_init_packet)) {
        LOG_ERROR("failed to send world init packet");
    }

    // Send game world objects to the new player
    i32 num_transfers = ((darray_length(game_world.objects) - 1) / MAX_GAME_OBJECTS_TRANSFER) + 1;
    u64 obj_count = darray_length(game_world.objects);
    for (i32 i = 0; i < num_transfers; i++) {
        i32 length;
        if (obj_count >= MAX_GAME_OBJECTS_TRANSFER) {
            length = MAX_GAME_OBJECTS_TRANSFER;
        } else {
            length = obj_count % MAX_GAME_OBJECTS_TRANSFER;
        }

        obj_count -= length;

        i32 offset = i * MAX_GAME_OBJECTS_TRANSFER;

        packet_world_object_add_t world_object_add_packet = {0};
        world_object_add_packet.length = length;

        memcpy(world_object_add_packet.objects, (game_world.objects + offset), length * sizeof(game_object_t));

        if (!packet_send(players[new_player_idx].socket, PACKET_TYPE_GAME_WORLD_OBJECT_ADD, &world_object_add_packet)) {
            LOG_ERROR("failed to send world add object packet (transfer number: %d)", i);
        }
    }
}

void handle_new_connection_request_event(void)
{
    struct sockaddr_storage client_addr;
    u32 client_addr_len = sizeof(client_addr);

    i32 client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_socket == -1) {
        LOG_ERROR("accept error: %s", strerror(errno));
        return;
    }

    char client_ip[INET6_ADDRSTRLEN] = {0};
    inet_ntop(client_addr.ss_family,
                get_in_addr((struct sockaddr *)&client_addr),
                client_ip,
                INET6_ADDRSTRLEN);

    u16 port = client_addr.ss_family == AF_INET ?
                ((struct sockaddr_in *)&client_addr)->sin_port :
                ((struct sockaddr_in6 *)&client_addr)->sin6_port;

    LOG_INFO("new connection from %s:%hu", client_ip, port);

    if (!validate_incoming_client(client_socket)) {
        LOG_ERROR("%s:%hu failed validation", client_ip, port);
        close(client_socket);
        return;
    }

    LOG_INFO("%s:%hu passed validation", client_ip, port);
    server_pfd_add(&fds, client_socket);
    handle_new_player_connection(client_socket);
}

b8 receive_client_data(i32 client_socket, u8 *recv_buffer, u32 buffer_size, i64 *bytes_read)
{
    ASSERT(recv_buffer);
    ASSERT(bytes_read);

    *bytes_read = net_recv(client_socket, recv_buffer, buffer_size, 0);
    if (*bytes_read <= 0) {
        if (*bytes_read == -1) {
            LOG_ERROR("recv error: %s", strerror(errno));
        } else if (*bytes_read == 0) {
            LOG_INFO("orderly shutdown");

            // Update other players that the user disconnected
            b8 found_player_to_remove = false;
            for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                if (players[i].socket == client_socket) {
                    found_player_to_remove = true;
                    if (players[i].id == PLAYER_INVALID_ID) { // Already removed by packet_player_remove
                        break;
                    }
                    LOG_INFO("removed player with id=%d", players[i].id);

                    packet_player_remove_t player_remove_packet = {
                        .id = players[i].id
                    };

                    packet_message_t message_packet = {0};
                    message_packet.type = MESSAGE_TYPE_SYSTEM;
                    snprintf(message_packet.content, sizeof(message_packet.content), "player <%s> left the game!", players[i].name);

                    for (i32 j = 0; j < MAX_PLAYER_COUNT; j++) {
                        if (players[j].id != PLAYER_INVALID_ID && players[j].id != players[i].id) {
                            if (!packet_send(players[j].socket, PACKET_TYPE_PLAYER_REMOVE, &player_remove_packet)) {
                                LOG_ERROR("failed to send player remove packet for player with id=%u", players[j].id);
                            }
                            if (!packet_send(players[j].socket, PACKET_TYPE_MESSAGE, &message_packet)) {
                                LOG_ERROR("failed to send message packet");
                            }
                        }
                    }

                    message_t msg = {0};
                    msg.type = MESSAGE_TYPE_SYSTEM;
                    memcpy(msg.content, message_packet.content, strlen(message_packet.content));
                    darray_push(messages, msg);

                    players[i].id = PLAYER_INVALID_ID;
                    break;
                }
            }

            if (!found_player_to_remove) {
                LOG_ERROR("failed to find player to remove after orderly shutdown");
            }

            server_pfd_remove(&fds, client_socket);
            close(client_socket);
        }

        return false;
    }

    return true;
}

void handle_packet_type(i32 client_socket, u8 *packet_body_buffer, u32 type, u32 *out_received_data_size)
{
    ASSERT(packet_body_buffer);

    u32 received_data_size = 0;
    switch (type) {
        case PACKET_TYPE_NONE: {
            LOG_WARN("received PACKET_TYPE_NONE, ignoring...");
        } break;
        case PACKET_TYPE_PING: { /* Bounce back the packet */
            received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PING];
            packet_ping_t *ping_packet = (packet_ping_t *)packet_body_buffer;
            if (!packet_send(client_socket, PACKET_TYPE_PING, ping_packet)) {
                LOG_ERROR("failed to send ping packet");
            }
        } break;
        case PACKET_TYPE_MESSAGE: {
            received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_MESSAGE];
            packet_message_t *message = (packet_message_t *)packet_body_buffer;
            LOG_INFO("author: %s, content: %s", message->author, message->content);

            time_t current_time = time(NULL);
            message->timestamp = (i64)current_time;

            message_t msg = {0};
            msg.type = MESSAGE_TYPE_PLAYER;
            memcpy(msg.author, message->author, strlen(message->author));
            memcpy(msg.content, message->content, strlen(message->content));
            darray_push(messages, msg);

            for (i32 i = 0; i < fds.count; i++) {
                if (fds.fds[i].fd == server_socket) {
                    continue;
                }
                if (!packet_send(fds.fds[i].fd, PACKET_TYPE_MESSAGE, message)) {
                    LOG_ERROR("failed to send message packet to client with socket fd=%d", fds.fds[i].fd);
                }
            }
        } break;
        case PACKET_TYPE_PLAYER_REMOVE: {
            received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_REMOVE];
            packet_player_remove_t *remove = (packet_player_remove_t *)packet_body_buffer;
            b8 found_player_to_remove = false;
            i32 player_idx;
            for (player_idx = 0; player_idx < MAX_PLAYER_COUNT; player_idx++) {
                if (players[player_idx].id == remove->id) {
                    players[player_idx].id = PLAYER_INVALID_ID;
                    found_player_to_remove = true;
                    break;
                }
            }
            if (!found_player_to_remove) {
                LOG_ERROR("could not find player to remove with id=%d", remove->id);
            } else {
                LOG_INFO("removed player with id=%d", remove->id);

                packet_message_t message_packet = {0};
                message_packet.type = MESSAGE_TYPE_SYSTEM;
                snprintf(message_packet.content, sizeof(message_packet.content), "player <%s> left the game!", players[player_idx].name);

                // Send updates to the rest of the players
                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (players[i].id != PLAYER_INVALID_ID && players[i].id != remove->id) {
                        if (!packet_send(players[i].socket, PACKET_TYPE_PLAYER_REMOVE, remove)) {
                            LOG_ERROR("failed to send player remove packet to player with id=%u", players[i].id);
                        }
                        if (!packet_send(players[i].socket, PACKET_TYPE_MESSAGE, &message_packet)) {
                            LOG_ERROR("failed to send message packet");
                        }
                    }
                }

                message_t msg = {0};
                msg.type = MESSAGE_TYPE_SYSTEM;
                memcpy(msg.content, message_packet.content, strlen(message_packet.content));
                darray_push(messages, msg);
            }
        } break;
        case PACKET_TYPE_PLAYER_UPDATE: {
            received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_UPDATE];
            packet_player_update_t *update = (packet_player_update_t *)packet_body_buffer;
            b8 found_player_to_update = false;
            for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                if (players[i].id == update->id) {
                    players[i].position = update->position;
                    found_player_to_update = true;
                    break;
                }
            }
            if (!found_player_to_update) {
                LOG_ERROR("could not find player to update with id=%d", update->id);
            } else {
                // Send updates to the rest of the players
                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (players[i].id != PLAYER_INVALID_ID && players[i].id != update->id) {
                        if (!packet_send(players[i].socket, PACKET_TYPE_PLAYER_UPDATE, update)) {
                            LOG_ERROR("failed to send player update packet to player with id=%u", players[i].id);
                        }
                    }
                }
            }
        } break;
        case PACKET_TYPE_PLAYER_KEYPRESS: {
            received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_KEYPRESS];
            packet_player_keypress_t *keypress = (packet_player_keypress_t *)packet_body_buffer;

            b8 enqueue_status;
            ring_buffer_enqueue(input_ring_buffer, *keypress, &enqueue_status);
            if (!enqueue_status) {
                LOG_ERROR("failed to enqueue new player input");
            }
        } break;
        default:
            LOG_WARN("received unknown packet type, ignoring...");
    }

    if (out_received_data_size) {
        *out_received_data_size = received_data_size;
    }
}

void handle_client_event(i32 client_socket)
{
    const u32 packet_header_size = PACKET_TYPE_SIZE[PACKET_TYPE_HEADER];

    i64 bytes_read;
    u8 recv_buffer[INPUT_BUFFER_SIZE + INPUT_OVERFLOW_BUFFER_SIZE] = {0};
    if (!receive_client_data(client_socket, recv_buffer, INPUT_BUFFER_SIZE, &bytes_read)) {
        return;
    }

    if (bytes_read < packet_header_size) { /* Check if at least 'header' amount of bytes were read */
        LOG_WARN("unimplemented: read less bytes than the header size"); /* TODO: Handle the case of having read less than header size */
    } else {
        // Check if multiple packets included in single tcp data reception
        u8 *buffer = recv_buffer;
        for (;;) {
            packet_header_t *header = (packet_header_t *)buffer;

            i32 packet_body_remaining_bytes = bytes_read - ((buffer - recv_buffer) + packet_header_size);
            // If not enough bytes were received in the buffer to make a complete packet body, read remaining amount of bytes
            if (packet_body_remaining_bytes < header->size) {
                ASSERT(INPUT_BUFFER_SIZE + INPUT_OVERFLOW_BUFFER_SIZE - (buffer - recv_buffer) >= packet_body_remaining_bytes);

                i64 num_remaining_body_bytes_read = 0;
                u8 *packet_body_buffer_ptr = buffer + packet_header_size + header->size - packet_body_remaining_bytes;
                if (!receive_client_data(client_socket, packet_body_buffer_ptr, packet_body_remaining_bytes, &num_remaining_body_bytes_read)) {
                    return;
                }

                ASSERT(num_remaining_body_bytes_read == packet_body_remaining_bytes);
                // At this point the rest of the packet body has been read and can be parsed based on header type
            }

            u32 received_packet_data_size;
            handle_packet_type(client_socket, buffer + packet_header_size, header->type, &received_packet_data_size);

            buffer += packet_header_size + received_packet_data_size;

            packet_header_t *next_header = (packet_header_t *)buffer;
            i32 bytes_remaining = bytes_read - (buffer - recv_buffer);

            if (bytes_remaining == 0) {
                // Finished parsing through all data in the receive buffer
                break;
            } else if (bytes_remaining < packet_header_size) {
                // Part of the header is inside of the buffer, so we need to read the remaining bytes of the header
                i32 header_bytes_left = packet_header_size - bytes_remaining;

                // Read remaining header bytes into the INPUT_OVERFLOW_BUFFER_SIZE memory
                i32 remaining_header_bytes_read = net_recv(client_socket, &recv_buffer[INPUT_BUFFER_SIZE], header_bytes_left, 0);
                UNUSED(remaining_header_bytes_read); // prevents compiler warning in release mode
                ASSERT(remaining_header_bytes_read == header_bytes_left);

                // Now the recv_buffer should be filled with additional 'header_bytes_left' bytes so we can inspect the header values
                // Finish the iteration by handling the packet type from the header, by reading additional header->size bytes and handling the packet
                u8 *next_packet_data_ptr = (u8 *)next_header + packet_header_size;
                received_packet_data_size = net_recv(client_socket, next_packet_data_ptr, next_header->size, 0);
                ASSERT(received_packet_data_size == next_header->size);

                handle_packet_type(client_socket, next_packet_data_ptr, next_header->type, NULL);
                break;
            } else if (next_header->type <= PACKET_TYPE_NONE || next_header->type >= PACKET_TYPE_COUNT) {
                LOG_WARN("received invalid packet type: %u", next_header->type);
                break;
            }
        }
    }
}

void signal_handler(i32 sig)
{
    running = false;
}

static b8 rect_collide(vec2 center1, vec2 size1, vec2 center2, vec2 size2)
{
    f32 left1 = center1.x - size1.x/2;
    f32 right1 = center1.x + size1.x/2;
    f32 top1 = center1.y - size1.y/2;
    f32 bottom1 = center1.y + size1.y/2;

    f32 left2 = center2.x - size2.x/2;
    f32 right2 = center2.x + size2.x/2;
    f32 top2 = center2.y - size2.y/2;
    f32 bottom2 = center2.y + size2.y/2;

    return left1 <= right2 && right1 >= left2 && top1 <= bottom2 && bottom1 >= top2;
}

static b8 is_player_key(u32 key)
{
    return key == KEYCODE_W || key == KEYCODE_S || key == KEYCODE_A || key == KEYCODE_D || key == KEYCODE_Space || key == KEYCODE_LeftShift;
}

static void process_player_input(u32 key, u32 mods, player_t *player, u32 damaged_players[MAX_PLAYER_COUNT])
{
    if (key == KEYCODE_LeftShift) {
        if (player->roll_cooldown > 0.0f) {
            LOG_WARN("received LeftShift keypress (roll) but roll_cooldown > 0");
            return;
        }
        player->state = PLAYER_STATE_ROLL;
        player->roll_cooldown = PLAYER_ROLL_COOLDOWN;
        player->roll_accumulator = 0.0f;
        if (player->direction == PLAYER_DIRECTION_UP) {
            player->roll_start = player->position.y;
            player->position.y += PLAYER_ROLL_DISTANCE;
        } else if (player->direction == PLAYER_DIRECTION_DOWN) {
            player->roll_start = player->position.y;
            player->position.y -= PLAYER_ROLL_DISTANCE;
        } else if (player->direction == PLAYER_DIRECTION_LEFT) {
            player->roll_start = player->position.x;
            player->position.x -= PLAYER_ROLL_DISTANCE;
        } else if (player->direction == PLAYER_DIRECTION_RIGHT) {
            player->roll_start = player->position.x;
            player->position.x += PLAYER_ROLL_DISTANCE;
        }
    } else {
        if (key == KEYCODE_Space) {
            if (player->attack_cooldown > 0.0f) {
                LOG_WARN("received Space keypress (attack) but attack_cooldown > 0");
                return;
            }
            player->state = PLAYER_STATE_ATTACK;
            player->attack_cooldown = PLAYER_ATTACK_COOLDOWN;
            player->attack_accumulator = 0.0f;

            static const f32 size = 32.0f;

            vec2 attack_center = player->position;
            vec2 attack_size = vec2_create(size, size);
            if (player->direction == PLAYER_DIRECTION_UP) {
                attack_center.y += size/2;
                attack_size.y /= 3.0f;
            } else if (player->direction == PLAYER_DIRECTION_DOWN) {
                attack_center.y -= size/2;
                attack_size.y /= 3.0f;
            } else if (player->direction == PLAYER_DIRECTION_LEFT) {
                attack_center.x -= size/2;
                attack_size.x /= 3.0f;
            } else if (player->direction == PLAYER_DIRECTION_RIGHT) {
                attack_center.x += size/2;
                attack_size.x /= 3.0f;
            }

            for (i32 j = 0; j < MAX_PLAYER_COUNT; j++) {
                if (players[j].id != PLAYER_INVALID_ID && players[j].id != player->id) {
                    player_t *other_player = &players[j];
                    if (rect_collide(attack_center, attack_size, other_player->position, vec2_create(size, size))) {
                        damaged_players[j] += PLAYER_DAMAGE_VALUE;
                    }
                }
            }
        }

        f32 velocity = CLIENT_TICK_DURATION * PLAYER_VELOCITY;
        if (key == KEYCODE_W) {
            player->position.y += velocity;
            player->position.y = (i32)player->position.y;
            if (player->state != PLAYER_STATE_ATTACK || player->direction != PLAYER_DIRECTION_UP) {
                player->state = PLAYER_STATE_WALK;
            }
            player->direction = PLAYER_DIRECTION_UP;
        } else if (key == KEYCODE_S) {
            player->position.y -= velocity;
            player->position.y = (i32)player->position.y;
            if (player->state != PLAYER_STATE_ATTACK || player->direction != PLAYER_DIRECTION_DOWN) {
                player->state = PLAYER_STATE_WALK;
            }
            player->direction = PLAYER_DIRECTION_DOWN;
        } else if (key == KEYCODE_A) {
            player->position.x -= velocity;
            player->position.x = (i32)player->position.x;
            if (player->state != PLAYER_STATE_ATTACK || player->direction != PLAYER_DIRECTION_LEFT) {
                player->state = PLAYER_STATE_WALK;
            }
            player->direction = PLAYER_DIRECTION_LEFT;
        } else if (key == KEYCODE_D) {
            player->position.x += velocity;
            player->position.x = (i32)player->position.x;
            if (player->state != PLAYER_STATE_ATTACK || player->direction != PLAYER_DIRECTION_RIGHT) {
                player->state = PLAYER_STATE_WALK;
            }
            player->direction = PLAYER_DIRECTION_RIGHT;
        }
    }
}

void process_pending_input(f64 delta_time)
{
    b8 dequeue_status;
    u32 processed_input_count = 0;
    packet_player_keypress_t keypress;
    u32 modified_players[MAX_PLAYER_COUNT] = {0};
    u32 damaged_players[MAX_PLAYER_COUNT] = {0};

    for (;;) {
        ring_buffer_dequeue(input_ring_buffer, &keypress, &dequeue_status);
        if (!dequeue_status) {
            break; // Finished processing all input from the queue
        }

        if (is_player_key(keypress.key)) {
            i32 sender_idx = -1;
            for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                if (players[i].id == keypress.id) {
                    sender_idx = i;
                    break;
                }
            }

            ASSERT(sender_idx != -1);
            process_player_input(keypress.key, keypress.mods, &players[sender_idx], damaged_players);

            players[sender_idx].seq_nr = keypress.seq_nr;

            if (modified_players[sender_idx] == 0) {
                modified_players[sender_idx] = 1;
            }
        }

        processed_input_count++;
        if (processed_input_count >= PROCESSED_INPUT_LIMIT_PER_UPDATE) {
            break;
        }
    }

    for (u64 i = 0; i < MAX_PLAYER_COUNT; i++) {
        player_t *player = &players[i];
        // Check if new input has been processed for a player
        if (modified_players[i] > 0) {
            vec2 position = player->position;
            // Roll was just initiated, so the update will contain new state and the initial roll position
            if (player->roll_cooldown == PLAYER_ROLL_COOLDOWN) {
                if (player->direction == PLAYER_DIRECTION_UP || player->direction == PLAYER_DIRECTION_DOWN) {
                    position.y = player->roll_start;
                } else if (player->direction == PLAYER_DIRECTION_LEFT || player->direction == PLAYER_DIRECTION_RIGHT) {
                    position.x = player->roll_start;
                }
            }

            // Send updated players' state to all other players including the sender
            packet_player_update_t player_update_packet = {
                .seq_nr    = player->seq_nr,
                .id        = player->id,
                .position  = position,
                .direction = player->direction,
                .state     = player->state
            };

            for (i32 j = 0; j < MAX_PLAYER_COUNT; j++) {
                if (players[j].id != PLAYER_INVALID_ID) {
                    if (!packet_send(players[j].socket, PACKET_TYPE_PLAYER_UPDATE, &player_update_packet)) {
                        LOG_ERROR("failed to send player update packet");
                    }
                }
            }
        }

        // Check if any damage was dealt to a player
        if (damaged_players[i] > 0 && player->health > 0) {
            player->health -= damaged_players[i];
            packet_player_death_t player_death_packet = {0};
            packet_message_t message_death_packet = {0};

            b8 player_died = player->health <= 0;

            if (player_died) {
                player->state = PLAYER_STATE_DEAD;
                player->respawn_cooldown = PLAYER_RESPAWN_COOLDOWN;
                player_death_packet.id = player->id;

                message_death_packet.type = MESSAGE_TYPE_SYSTEM;
                snprintf(message_death_packet.content,
                         sizeof(message_death_packet.content),
                         "player <%s> died! respawning in %.2f seconds...",
                         player->name, PLAYER_RESPAWN_COOLDOWN);

                message_t msg = {0};
                msg.type = MESSAGE_TYPE_SYSTEM;
                memcpy(msg.content, message_death_packet.content, strlen(message_death_packet.content));
                darray_push(messages, msg);
            }

            // Send health updates to all players
            packet_player_health_t player_health_packet = {
                .id     = player->id,
                .damage = damaged_players[i]
            };

            for (i32 j = 0; j < MAX_PLAYER_COUNT; j++) {
                if (players[j].id != PLAYER_INVALID_ID) {
                    if (!packet_send(players[j].socket, PACKET_TYPE_PLAYER_HEALTH, &player_health_packet)) {
                        LOG_ERROR("failed to send player health packet");
                    }

                    if (player_died) {
                        if (!packet_send(players[j].socket, PACKET_TYPE_PLAYER_DEATH, &player_death_packet)) {
                            LOG_ERROR("failed to send player death packet");
                        }
                        if (!packet_send(players[j].socket, PACKET_TYPE_MESSAGE, &message_death_packet)) {
                            LOG_ERROR("failed to send message death packet");
                        }
                    }
                }
            }
        }
    }

    for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
        player_t *player = &players[i];
        if (player->id != PLAYER_INVALID_ID) {
            b8 should_update = false;
            // Check if any players are dead and should be respawned
            if (player->state == PLAYER_STATE_DEAD) {
                if (player->respawn_cooldown <= 0.0f) {
                    // Found player who should be respawned
                    // Update player state and send respawn packet to all players
                    player->state = PLAYER_STATE_IDLE;
                    player->health = PLAYER_START_HEALTH;
                    player->position = PLAYER_SPAWN_POSITION;
                    player->direction = PLAYER_DIRECTION_DOWN;

                    packet_player_respawn_t player_respawn_packet = {
                        .id        = player->id,
                        .health    = player->health,
                        .state     = player->state,
                        .position  = player->position,
                        .direction = player->direction
                    };

                    for (i32 j = 0; j < MAX_PLAYER_COUNT; j++) {
                        if (players[j].id != PLAYER_INVALID_ID) {
                            if (!packet_send(players[j].socket, PACKET_TYPE_PLAYER_RESPAWN, &player_respawn_packet)) {
                                LOG_ERROR("failed to send player health packet");
                            }
                        }
                    }
                } else {
                    player->respawn_cooldown -= delta_time;
                }
            } else if (player->state == PLAYER_STATE_ROLL) {
                player->roll_accumulator += delta_time;
                if (player->roll_accumulator >= PLAYER_ROLL_DURATION) {
                    player->roll_accumulator = 0.0f;
                    player->state = PLAYER_STATE_IDLE;
                    should_update = true;
                }
            } else if (player->state == PLAYER_STATE_ATTACK) {
                player->attack_accumulator += delta_time;
                if (player->attack_accumulator >= PLAYER_ATTACK_DURATION) {
                    player->attack_accumulator = 0.0f;
                    player->state = PLAYER_STATE_IDLE;
                    should_update = true;
                }
            }

            if (should_update) {
                packet_player_update_t player_update_packet = {
                    .seq_nr    = player->seq_nr,
                    .id        = player->id,
                    .position  = player->position,
                    .direction = player->direction,
                    .state     = player->state
                };

                for (i32 j = 0; j < MAX_PLAYER_COUNT; j++) {
                    if (players[j].id != PLAYER_INVALID_ID && players[j].id != player->id) {
                        if (!packet_send(players[j].socket, PACKET_TYPE_PLAYER_UPDATE, &player_update_packet)) {
                            LOG_ERROR("failed to send player update packet");
                        }
                    }
                }
            }

            // Update cooldowns
            if (player->attack_cooldown > 0.0f) {
                player->attack_cooldown -= delta_time;
            }
            if (player->roll_cooldown > 0.0f) {
                player->roll_cooldown -= delta_time;
            }
        }
    }
}

void *process_input_queue(void *args)
{
    static const u32 us_to_sleep = 1.0f / SERVER_TICK_RATE * 1000 * 1000;
    static const f64 delta_time = 1.0 / SERVER_TICK_RATE;
    while (running) {
        process_pending_input(delta_time);
        net_update(delta_time);
        usleep(us_to_sleep);
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        LOG_FATAL("usage: %s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char hostname[256] = {0};
    gethostname(hostname, 256);
    LOG_INFO("starting the game server on host '%s'", hostname);

    struct addrinfo hints;
    struct addrinfo *result, *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM; /* Stream socket */
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_flags = AI_PASSIVE;     /* For wildcard IP addresses */

    i32 status = getaddrinfo(NULL, argv[1], &hints, &result);
    if (status != 0) {
        LOG_FATAL("getaddrinfo error: %s", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        server_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (server_socket == -1) {
            continue;
        }

        const char *ip_version = rp->ai_family == AF_INET  ? "IPv4" :
                                 rp->ai_family == AF_INET6 ? "IPv6" : "UNKNOWN";
        const char *socktype_str = rp->ai_socktype == SOCK_STREAM ? "TCP" :
                                   rp->ai_socktype == SOCK_DGRAM  ? "UDP" : "UNKNOWN";
        LOG_INFO("successfully created an %s %s socket(fd=%d)", ip_version, socktype_str, server_socket);

        static i32 yes = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(i32)) == -1) {
            LOG_ERROR("setsockopt reuse address error: %s", strerror(errno));
        }

        if (bind(server_socket, rp->ai_addr, rp->ai_addrlen) == 0) {
            LOG_INFO("successfully bound socket(fd=%d)", server_socket);
            break;
        } else {
            LOG_ERROR("failed to bind socket with fd=%d", server_socket);
            close(server_socket);
        }
    }

    if (rp == NULL) { /* No address succeeded */
        LOG_FATAL("could not bind to port %s", argv[1]);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, SERVER_BACKLOG) == -1) {
        LOG_FATAL("failed to start listening: %s", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        char ip_buffer[INET6_ADDRSTRLEN] = {0};
        inet_ntop(rp->ai_family, get_in_addr(rp->ai_addr), ip_buffer, INET6_ADDRSTRLEN);
        LOG_INFO("socket listening on %s port %s", ip_buffer, argv[1]);
    }

    freeaddrinfo(result);

    server_pfd_init(5, &fds);
    server_pfd_add(&fds, server_socket);

    input_ring_buffer = ring_buffer_reserve(INPUT_RING_BUFFER_CAPACITY, sizeof(packet_player_keypress_t));
    messages = darray_create(sizeof(message_t));

    // Initialize game world
    game_world.map.seed = math_random();
    game_world.map.octave_count = 2;
    game_world.map.bias = 2.0f;
    game_world.objects = darray_create(sizeof(game_object_t));

#if 0
    f32 *perlin_noise_data = malloc(game_world.map.width * game_world.map.height * sizeof(f32));

    perlin_noise_config_t config = {
        .width = game_world.map.width,
        .height = game_world.map.height,
        .seed = game_world.map.seed,
        .octave_count = game_world.map.octave_count,
        .scaling_bias = game_world.map.bias
    };

    perlin_noise_generate_2d(config, perlin_noise_data);

    for (i32 i = 0; i < game_world.map.width * game_world.map.height; i++) {
        if (perlin_noise_data[i] >= 0.45f && perlin_noise_data[i] < 0.8f) {
            // Spawn random vegetation on grass tiles
            f32 r = math_frandom();
            if (0.0f <= r && r < 0.01f) { // ~1% chance of spawning a bush
                game_object_t obj = {
                    .type = GAME_OBJECT_TYPE_BUSH,
                    .tile_index = i
                };
                darray_push(game_world.objects, obj);
            }
        } else if (perlin_noise_data[i] < 0.4f) {
            // Spawn lilies on water tiles
            f32 r = math_frandom();
            if (0.0f <= r && r < 0.01f) { // ~1% chance of spawning a lily
                game_object_t obj = {
                    .type = GAME_OBJECT_TYPE_LILY,
                    .tile_index = i
                };
                darray_push(game_world.objects, obj);
            }
        }
    }

    free(perlin_noise_data);
#endif

    struct sigaction sa = {0};
    sa.sa_flags = SA_RESTART; // Restart functions interruptable by EINTR like poll()
    sa.sa_handler = &signal_handler;
    sigaction(SIGINT, &sa, NULL);

    running = true;

    pthread_t input_queue_processing_thread;
    pthread_create(&input_queue_processing_thread, NULL, process_input_queue, NULL);

    LOG_INFO("server tick rate: %u", SERVER_TICK_RATE);

    while (running) {
        i32 num_events = poll(fds.fds, fds.count, POLL_INFINITE_TIMEOUT);

        if (num_events == -1) {
            if (errno == EINTR) {
                LOG_TRACE("interrupted 'poll' system call");
                break;
            }
            LOG_FATAL("poll error: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        for (i32 i = 0; i < fds.count; i++) {
            if (fds.fds[i].revents & POLLIN) {
                if (fds.fds[i].fd == server_socket) { /* New connection request */
                    handle_new_connection_request_event();
                    break;
                } else { /* Client trying to send data */
                    handle_client_event(fds.fds[i].fd);
                }
            }
        }
    }

    LOG_INFO("server shutting down");

    // TODO: Permanently store messages to disk. For now just delete them all
    darray_destroy(messages);

    server_pfd_shutdown(&fds);

    pthread_join(input_queue_processing_thread, NULL);
    LOG_INFO("shut down input queue processing thread");

    if (close(server_socket) == -1) {
        LOG_ERROR("error while closing the socket: %s", strerror(errno));
    }

    return EXIT_SUCCESS;
}
