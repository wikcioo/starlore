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
#include <time.h>
#include <errno.h>
#include <signal.h>

#include "defines.h"
#include "common/config.h"
#include "common/packet.h"
#include "common/logger.h"
#include "common/asserts.h"
#include "common/maths.h"
#include "common/input_codes.h"

#define INPUT_BUFFER_SIZE 1024
#define INPUT_OVERFLOW_BUFFER_SIZE 256

#define SERVER_BACKLOG 10
#define POLL_INFINITE_TIMEOUT -1

typedef struct server_pfd {
    struct pollfd *fds;
    u32 count;
    u32 capacity;
} server_pfd_t;

typedef struct player {
    i32 socket;
    player_id id;
    vec2 position;
    vec3 color;
} player_t;

static b8 running;
static i32 server_socket;
static server_pfd_t fds;
static player_t players[MAX_PLAYER_COUNT];
static player_id current_player_id = 1000;

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

b8 validate_incoming_client(i32 client_socket)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    u64 puzzle_value = (u64)ts.tv_nsec;

    i64 bytes_read, bytes_sent;

    bytes_sent = send(client_socket, (void *)&puzzle_value, sizeof(puzzle_value), 0);
    if (bytes_sent == -1) {
        LOG_ERROR("validation: send error: %s", strerror(errno));
        return false;
    } else if (bytes_sent != sizeof(puzzle_value)) {
        LOG_ERROR("validation: failed to send %lu bytes of validation data", sizeof(puzzle_value));
        return false;
    }

    u64 answer = puzzle_value ^ 0xDEADBEEFCAFEBABE; /* TODO: Come up with a better validation function */

    u64 result_buffer;
    bytes_read = recv(client_socket, (void *)&result_buffer, sizeof(result_buffer), 0);
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
        bytes_sent = send(client_socket, (void *)&status_buffer, sizeof(status_buffer), 0);
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
    f32 red   = maths_frandom_range(0.0, 1.0);
    f32 green = maths_frandom_range(0.0, 1.0);
    f32 blue  = maths_frandom_range(0.0, 1.0);
    for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].id == PLAYER_INVALID_ID) { /* Free slot */
            players[i].socket   = client_socket;
            players[i].id       = current_player_id;
            players[i].position = vec2_create(0.0f, 0.0f);
            players[i].color    = vec3_create(red, green, blue);
            break;
        }
    }

    packet_player_init_t player_init_packet = {
        .id       = current_player_id,
        .position = vec2_create(0.0f, 0.0f),
        .color    = vec3_create(red, green, blue)
    };
    if (!packet_send(client_socket, PACKET_TYPE_PLAYER_INIT, &player_init_packet)) {
        LOG_ERROR("failed to send player init packet");
    }

    current_player_id++;

    // Send the rest of the players to the new player
    for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].id != PLAYER_INVALID_ID && players[i].id != player_init_packet.id) {
            packet_player_add_t player_add_packet = {
                .id       = players[i].id,
                .position = players[i].position,
                .color    = players[i].color
            };
            if (!packet_send(client_socket, PACKET_TYPE_PLAYER_ADD, &player_add_packet)) {
                LOG_ERROR("failed to send player add packet");
            }
        }
    }

    // Update other players with the new player
    for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
        if (players[i].id != PLAYER_INVALID_ID && players[i].id != player_init_packet.id) {
            packet_player_add_t player_add_packet = {
                .id       = player_init_packet.id,
                .position = player_init_packet.position,
                .color    = player_init_packet.color
            };
            if (!packet_send(players[i].socket, PACKET_TYPE_PLAYER_ADD, &player_add_packet)) {
                LOG_ERROR("failed to send player add packet");
            }
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

    *bytes_read = recv(client_socket, recv_buffer, INPUT_BUFFER_SIZE, 0);
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

                    for (i32 j = 0; j < MAX_PLAYER_COUNT; j++) {
                        if (players[j].id != PLAYER_INVALID_ID && players[j].id != players[i].id) {
                            if (!packet_send(players[j].socket, PACKET_TYPE_PLAYER_REMOVE, &player_remove_packet)) {
                                LOG_ERROR("failed to send player remove packet for player with id=%u", players[j].id);
                            }
                        }
                    }

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

            for (i32 i = 0; i < fds.count; i++) {
                if (/*fds.fds[i].fd == client_socket || */fds.fds[i].fd == server_socket) {
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
            for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                if (players[i].id == remove->id) {
                    players[i].id = PLAYER_INVALID_ID;
                    found_player_to_remove = true;
                    break;
                }
            }
            if (!found_player_to_remove) {
                LOG_ERROR("could not find player to remove with id=%d", remove->id);
            } else {
                LOG_INFO("removed player with id=%d", remove->id);

                // Send updates to the rest of the players
                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (players[i].id != PLAYER_INVALID_ID && players[i].id != remove->id) {
                        if (!packet_send(players[i].socket, PACKET_TYPE_PLAYER_REMOVE, remove)) {
                            LOG_ERROR("failed to send player remove packet to player with id=%u", players[i].id);
                        }
                    }
                }
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

            if (keypress->key == KEYCODE_W || keypress->key == KEYCODE_S || keypress->key == KEYCODE_A || keypress->key == KEYCODE_D) {
                // Update player position and send updates to all players including the sender
                i32 sender_idx = -1;
                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (players[i].socket == client_socket) {
                        sender_idx = i;
                    }
                }

                ASSERT(sender_idx != -1);

                if (keypress->key == KEYCODE_W) {
                    players[sender_idx].position.y += PLAYER_VELOCITY;
                } else if (keypress->key == KEYCODE_S) {
                    players[sender_idx].position.y -= PLAYER_VELOCITY;
                } else if (keypress->key == KEYCODE_A) {
                    players[sender_idx].position.x -= PLAYER_VELOCITY;
                } else if (keypress->key == KEYCODE_D) {
                    players[sender_idx].position.x += PLAYER_VELOCITY;
                }

                packet_player_update_t player_update_packet = {
                    .seq_nr = keypress->seq_nr,
                    .id = players[sender_idx].id,
                    .position = players[sender_idx].position
                };

                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (players[i].id != PLAYER_INVALID_ID) {
                        if (!packet_send(players[i].socket, PACKET_TYPE_PLAYER_UPDATE, &player_update_packet)) {
                            LOG_ERROR("failed to send player update packet");
                        }
                    }
                }
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
                i32 remaining_header_bytes_read = recv(client_socket, &recv_buffer[INPUT_BUFFER_SIZE], header_bytes_left, 0);
                UNUSED(remaining_header_bytes_read); // prevents compiler warning in release mode
                ASSERT(remaining_header_bytes_read == header_bytes_left);

                // Now the recv_buffer should be filled with additional 'header_bytes_left' bytes so we can inspect the header values
                // Finish the iteration by handling the packet type from the header, by reading additional header->size bytes and handling the packet
                u8 *next_packet_data_ptr = (u8 *)next_header + packet_header_size;
                received_packet_data_size = recv(client_socket, next_packet_data_ptr, next_header->size, 0);
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

    struct sigaction sa = {0};
    sa.sa_flags = SA_RESTART; // Restart functions interruptable by EINTR like poll()
    sa.sa_handler = &signal_handler;
    sigaction(SIGINT, &sa, NULL);

    running = true;

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
                } else { /* Client trying to send data */
                    handle_client_event(fds.fds[i].fd);
                }
            }
        }
    }

    LOG_INFO("server shutting down");

    server_pfd_shutdown(&fds);

    if (close(server_socket) == -1) {
        LOG_ERROR("error while closing the socket: %s", strerror(errno));
    }

    return EXIT_SUCCESS;
}
