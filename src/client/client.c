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
#include "color_palette.h"
#include "common/asserts.h"
#include "common/strings.h"
#include "common/global.h"
#include "common/packet.h"
#include "common/logger.h"
#include "common/maths.h"
#include "common/input_codes.h"
#include "common/containers/ring_buffer.h"

#define POLLFD_COUNT 2
#define INPUT_BUFFER_SIZE 4096
#define OVERFLOW_BUFFER_SIZE 1024
#define RING_BUFFER_CAPACITY 256
#define POLL_INFINITE_TIMEOUT -1

#define CLIENT_TICK_DURATION (1.0f / CLIENT_TICK_RATE)
#define SERVER_TICK_DURATION (1.0f / SERVER_TICK_RATE)

typedef struct {
    player_id id;
    char name[MAX_PLAYER_NAME_LENGTH];
    vec2 position;
    vec2 last_position; // Used for interpolation
    b8 interp_complete;
    vec3 color;
} player_t;

static u32 other_player_count = 0;
static player_t other_players[MAX_PLAYER_COUNT];
static f32 server_update_accumulator = 0.0f;
static player_t self_player;
static b8 keys[KEYCODE_Last];
static struct pollfd pfds[POLLFD_COUNT];
static char input_buffer[INPUT_BUFFER_SIZE] = {0};
static u32 input_count = 0;
static b8 running = false;
static void *ring_buffer;
static u32 current_sequence_number = 1;

// Data referenced from somewhere else
char username[MAX_PLAYER_NAME_LENGTH];
vec2 current_window_size;
i32 client_socket;
GLFWwindow *main_window;

texture_t player_texture;

b8 handle_client_validation(i32 client)
{
    u64 puzzle_buffer;
    i64 bytes_read, bytes_sent;

    bytes_read = recv(client, (void *)&puzzle_buffer, sizeof(puzzle_buffer), 0); /* TODO: Handle unresponsive server */
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
        bytes_sent = send(client, (void *)&answer, sizeof(answer), 0);
        if (bytes_sent == -1) {
            LOG_ERROR("validation: send error: %s", strerror(errno));
            return false;
        } else if (bytes_sent != sizeof(answer)) {
            LOG_ERROR("validation: failed to send %lu bytes of validation data", sizeof(answer));
            return false;
        }

        b8 status_buffer;
        bytes_read = recv(client, (void *)&status_buffer, sizeof(status_buffer), 0);
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

void send_ping_packet(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    u64 time_now = (u64)(ts.tv_sec * 1000000000 + ts.tv_nsec);

    packet_ping_t ping_packet = {
        .time = time_now
    };
    if (!packet_send(client_socket, PACKET_TYPE_PING, &ping_packet)) {
        LOG_ERROR("failed to send ping packet");
    }
}

void handle_stdin_event(void)
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
            memset(input_buffer, 0, INPUT_BUFFER_SIZE);
        } else if (input_count > 0) {
            char *input_trimmed = string_trim(input_buffer);
            if (strlen(input_trimmed) < 1) {
                return;
            }

            packet_message_t message_packet = {0};
            message_packet.type = MESSAGE_TYPE_PLAYER;
            u32 username_size = strlen(username) > MAX_PLAYER_NAME_LENGTH ? MAX_PLAYER_NAME_LENGTH : strlen(username);
            strncpy(message_packet.author, username, username_size);
            u32 content_size = input_count > MAX_MESSAGE_CONTENT_LENGTH ? MAX_MESSAGE_CONTENT_LENGTH : input_count;
            strncpy(message_packet.content, input_trimmed, content_size);

            if (!packet_send(client_socket, PACKET_TYPE_MESSAGE, &message_packet)) {
                LOG_ERROR("failed to send message packet");
            }

            input_count = 0;
            memset(input_buffer, 0, INPUT_BUFFER_SIZE);
        }
    }
}

void handle_socket_event(void)
{
    u8 recv_buffer[INPUT_BUFFER_SIZE + OVERFLOW_BUFFER_SIZE] = {0};

    i64 bytes_read = recv(client_socket, recv_buffer, INPUT_BUFFER_SIZE, 0);
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
            i64 new_bytes_read = recv(client_socket, &recv_buffer[INPUT_BUFFER_SIZE], missing_bytes, 0);
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

                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                u64 time_now = (u64)(ts.tv_sec * 1000000000 + ts.tv_nsec);

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
                    memcpy(msg.name, message->author, strlen(message->author));
                    memcpy(msg.content, message->content, strlen(message->content));
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
                        memcpy(msg.name, message.author, strlen(message.author));
                        memcpy(msg.content, message.content, strlen(message.content));
                        chat_add_player_message(msg);
                    } else {
                        LOG_ERROR("unknown history message type %d", message.type);
                    }
                }
            } break;
            case PACKET_TYPE_PLAYER_INIT: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_INIT];
                packet_player_init_t *player_init = (packet_player_init_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);
                self_player.id       = player_init->id;
                self_player.position = player_init->position;
                self_player.color    = player_init->color;
                LOG_INFO("initialized self: id=%u position=(%f,%f) color=(%f,%f,%f)",
                        self_player.id,
                        self_player.position.x, self_player.position.y,
                        self_player.color.r, self_player.color.g, self_player.color.b);

                packet_player_init_confirm_t player_confirm_packet = {0};
                player_confirm_packet.id = self_player.id;
                memcpy(player_confirm_packet.name, username, strlen(username));
                if (!packet_send(client_socket, PACKET_TYPE_PLAYER_INIT_CONF, &player_confirm_packet)) {
                    LOG_ERROR("failed to send player init confirm packet");
                }
            } break;
            case PACKET_TYPE_PLAYER_ADD: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_ADD];
                packet_player_add_t *player_add = (packet_player_add_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);

                b8 found_free_slot = false;
                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (other_players[i].id == PLAYER_INVALID_ID) { /* Free slot */
                        LOG_INFO("adding new player id=%u", player_add->id);
                        other_players[i].id              = player_add->id;
                        other_players[i].position        = player_add->position;
                        other_players[i].color           = player_add->color;
                        other_players[i].last_position   = player_add->position;
                        other_players[i].interp_complete = true;
                        memcpy(other_players[i].name, player_add->name, strlen(player_add->name));
                        other_player_count++;
                        found_free_slot = true;
                        break;
                    }
                }
                if (!found_free_slot) {
                    LOG_ERROR("failed to add new player, no free slots - other_player_count=%u", other_player_count);
                }
            } break;
            case PACKET_TYPE_PLAYER_REMOVE: {
                received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_REMOVE];
                packet_player_remove_t *player_remove = (packet_player_remove_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);
                b8 found_player_to_remove = false;
                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (other_players[i].id == player_remove->id) {
                        other_players[i].id = PLAYER_INVALID_ID;
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

                if (player_update->id == self_player.id) {
                    for (;;) {
                        b8 dequeue_status;
                        packet_player_keypress_t keypress = {0};

                        ring_buffer_dequeue(ring_buffer, &keypress, &dequeue_status);
                        if (!dequeue_status) {
                            LOG_WARN("ran out of keypresses and did not find appropriate sequence number");
                            break;
                        }

                        if (keypress.seq_nr == player_update->seq_nr) {
                            // Got authoritative position update from the server, so update self_player position received from the server
                            // and re-apply all keypresses that happened since then
                            self_player.position = player_update->position;

                            u32 nth_element = 0;
                            while (ring_buffer_peek_from_end(ring_buffer, nth_element++, &keypress)) {
                                if (keypress.key == KEYCODE_W) {
                                    self_player.position.y += PLAYER_VELOCITY;
                                } else if (keypress.key == KEYCODE_S) {
                                    self_player.position.y -= PLAYER_VELOCITY;
                                } else if (keypress.key == KEYCODE_A) {
                                    self_player.position.x -= PLAYER_VELOCITY;
                                } else if (keypress.key == KEYCODE_D) {
                                    self_player.position.x += PLAYER_VELOCITY;
                                }
                            }
                            break;
                        }
                    }
                    break;
                }

                b8 found_player_to_update = false;
                for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                    if (other_players[i].id == player_update->id) {
                        other_players[i].last_position = other_players[i].position;
                        other_players[i].position = player_update->position;
                        other_players[i].interp_complete = false;
                        found_player_to_update = true;
                        server_update_accumulator = 0.0f;
                        break;
                    }
                }
                if (!found_player_to_update) {
                    LOG_ERROR("failed to update player with id=%u", player_update->id);
                }
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

void *handle_networking(void *args)
{
    while (running) {
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

void glfw_error_callback(i32 code, const char *description)
{
    LOG_ERROR("glfw error code: %d (%s)", code, description);
}

void glfw_framebuffer_size_callback(GLFWwindow *window, i32 width, i32 height)
{
    event_system_fire(EVENT_CODE_WINDOW_RESIZED, (event_data_t){ .u32[0]=width, .u32[1]=height });
}

void glfw_window_close_callback(GLFWwindow *window)
{
    event_system_fire(EVENT_CODE_WINDOW_CLOSED, (event_data_t){0});
}

void glfw_key_callback(GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods)
{
    event_data_t data = {0};
    data.u16[0] = key;
    data.u16[1] = mods;

    if (action == INPUTACTION_Press) {
        event_system_fire(EVENT_CODE_KEY_PRESSED, data);
    } else if (action == INPUTACTION_Release) {
        event_system_fire(EVENT_CODE_KEY_RELEASED, data);
    } else if (action == INPUTACTION_Repeat) {
        event_system_fire(EVENT_CODE_KEY_REPEATED, data);
    }
}

void glfw_char_callback(GLFWwindow *window, u32 codepoint)
{
    event_system_fire(EVENT_CODE_CHAR_PRESSED, (event_data_t){ .u32[0]=codepoint });
}

void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (action == INPUTACTION_Press) {
        event_system_fire(EVENT_CODE_MOUSE_BUTTON_PRESSED, (event_data_t){ .u8[0]=button });
    }
}

b8 key_pressed_event_callback(event_code_e code, event_data_t data)
{
    u16 key = data.u16[0];
    if (key == KEYCODE_P) {
        send_ping_packet();
    } else {
        keys[key] = true;
    }

    return true;
}

b8 key_released_event_callback(event_code_e code, event_data_t data)
{
    u16 key = data.u16[0];
    keys[key] = false;
    return true;
}

b8 window_closed_event_callback(event_code_e code, event_data_t data)
{
    running = false;
    return true;
}

b8 window_resized_event_callback(event_code_e code, event_data_t data)
{
    u32 width = data.u32[0];
    u32 height = data.u32[1];
    current_window_size = vec2_create(width, height);
    glViewport(0, 0, width, height);
    return false;
}

void update_self_player(f64 delta_time)
{
    if (keys[KEYCODE_W] || keys[KEYCODE_S] || keys[KEYCODE_A] || keys[KEYCODE_D]) {
        i32 key = 0;
        if (keys[KEYCODE_W]) {
            key = KEYCODE_W;
        } else if (keys[KEYCODE_S]) {
            key = KEYCODE_S;
        } else if (keys[KEYCODE_A]) {
            key = KEYCODE_A;
        } else if (keys[KEYCODE_D]) {
            key = KEYCODE_D;
        }

        { // client-side prediction
            vec2 movement = vec2_zero();
            if (key == KEYCODE_W) {
                movement.y += PLAYER_VELOCITY;
            }
            if (key == KEYCODE_S) {
                movement.y -= PLAYER_VELOCITY;
            }
            if (key == KEYCODE_A) {
                movement.x -= PLAYER_VELOCITY;
            }
            if (key == KEYCODE_D) {
                movement.x += PLAYER_VELOCITY;
            }

            self_player.position = vec2_add(self_player.position, movement);
        }

        packet_player_keypress_t player_keypress_packet = {
            .id = self_player.id,
            .seq_nr = current_sequence_number++,
            .key = key,
            .action = INPUTACTION_Press
        };

        b8 enqueue_status;
        ring_buffer_enqueue(ring_buffer, player_keypress_packet, &enqueue_status);
        if (!enqueue_status) {
            LOG_ERROR("failed to enqueue player keypress packet");
            return;
        }

        if (!packet_send(client_socket, PACKET_TYPE_PLAYER_KEYPRESS, &player_keypress_packet)) {
            LOG_ERROR("failed to send player keypress packet");
        }
    }
}

void signal_handler(i32 sig)
{
    running = false;
}

void display_build_version(void)
{
    renderer_draw_text(BUILD_VERSION(CLIENT_VERSION_MAJOR, CLIENT_VERSION_MINOR, CLIENT_VERSION_PATCH),
                       FA32,
                       vec2_create(3.0f, current_window_size.y - renderer_get_font_height(FA32)),
                       1.0f,
                       COLOR_MILK,
                       0.6f);
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        LOG_FATAL("usage: %s ip port username", argv[0]);
        exit(EXIT_FAILURE);
    }

    memcpy(username, argv[3], strlen(argv[3]));

    if (glfwInit() != GLFW_TRUE) {
        LOG_FATAL("failed to initialize glfw");
        exit(EXIT_FAILURE);
    }

    glfwSetErrorCallback(glfw_error_callback);

    main_window = glfwCreateWindow(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, "Chat Client", NULL, NULL);
    if (main_window == NULL) {
        LOG_FATAL("failed to create glfw window");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    current_window_size = vec2_create((f32)DEFAULT_WINDOW_WIDTH, (f32)DEFAULT_WINDOW_HEIGHT);

    glfwMakeContextCurrent(main_window);
    glfwSetFramebufferSizeCallback(main_window, glfw_framebuffer_size_callback);
    glfwSetWindowCloseCallback(main_window, glfw_window_close_callback);
    glfwSetKeyCallback(main_window, glfw_key_callback);
    glfwSetCharCallback(main_window, glfw_char_callback);
    glfwSetMouseButtonCallback(main_window, glfw_mouse_button_callback);

    glfwSwapInterval(VSYNC_ENABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG_FATAL("failed to load opengl");
        glfwDestroyWindow(main_window);
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    const GLFWvidmode *vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    LOG_INFO("primary monitor parameters:\n\t  width: %d\n\t  height: %d\n\t  refresh rate: %d",
             vidmode->width, vidmode->height, vidmode->refreshRate);

    LOG_INFO("graphics info:\n\t  vendor: %s\n\t  renderer: %s\n\t  version: %s",
             glGetString(GL_VENDOR), glGetString(GL_RENDERER), glGetString(GL_VERSION));
    LOG_INFO("running opengl version %d.%d", GLVersion.major, GLVersion.minor);
    LOG_INFO("running glfw version %s", glfwGetVersionString());
    LOG_INFO("vsync: %s", VSYNC_ENABLED ? "on" : "off");

    if (!event_system_init()) {
        LOG_ERROR("failed to initialize event system");
        exit(EXIT_FAILURE);
    }

    if (!renderer_init()) {
        LOG_ERROR("failed to initialize renderer");
        exit(EXIT_FAILURE);
    }

    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    i32 status_code = getaddrinfo(argv[1], argv[2], &hints, &result);
    if (status_code != 0) {
        LOG_FATAL("getaddrinfo error: %s", gai_strerror(status_code));
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        client_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (client_socket == -1) {
            continue;
        }

        if (connect(client_socket, rp->ai_addr, rp->ai_addrlen) == -1) {
            LOG_ERROR("connect error: %s", strerror(errno)); /* TODO: Provide more information about failed parameters */
            continue;
        }

        break;
    }

    freeaddrinfo(result);

    if (rp == NULL) {
        LOG_FATAL("failed to connect");
        exit(EXIT_FAILURE);
    }

    LOG_INFO("connected to server at %s:%s", argv[1], argv[2]);

    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;

    pfds[1].fd = client_socket;
    pfds[1].events = POLLIN;

    if (!handle_client_validation(client_socket)) {
        LOG_FATAL("failed client validation");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    LOG_INFO("client successfully validated");

    running = true;

    struct sigaction sa;
    sa.sa_flags = SA_RESTART; // Restart functions interruptable by EINTR like poll()
    sa.sa_handler = &signal_handler;
    sigaction(SIGINT, &sa, NULL);

    ring_buffer = ring_buffer_reserve(RING_BUFFER_CAPACITY, sizeof(packet_player_keypress_t));

    chat_init();

    texture_create_from_path("assets/textures/old_man.png", &player_texture);

    event_system_register(EVENT_CODE_CHAR_PRESSED, chat_char_pressed_event_callback);
    event_system_register(EVENT_CODE_KEY_PRESSED, chat_key_pressed_event_callback);
    event_system_register(EVENT_CODE_KEY_REPEATED, chat_key_repeated_event_callback);

    event_system_register(EVENT_CODE_KEY_PRESSED, key_pressed_event_callback);
    event_system_register(EVENT_CODE_KEY_RELEASED, key_released_event_callback);

    event_system_register(EVENT_CODE_MOUSE_BUTTON_PRESSED, chat_mouse_button_pressed_event_callback);

    event_system_register(EVENT_CODE_WINDOW_CLOSED, window_closed_event_callback);
    event_system_register(EVENT_CODE_WINDOW_RESIZED, window_resized_event_callback);

    pthread_t network_thread;
    pthread_create(&network_thread, NULL, handle_networking, NULL);

    f64 last_time = glfwGetTime();
    f64 delta_time = 0.0f;

    f64 client_update_accumulator = 0.0f;

    LOG_INFO("server tick rate: %u", SERVER_TICK_RATE);

    while (running) {
        f64 now = glfwGetTime();
        delta_time = now - last_time;
        last_time = now;

        server_update_accumulator += delta_time;
        client_update_accumulator += delta_time;

        if (client_update_accumulator >= CLIENT_TICK_DURATION) {
            update_self_player(delta_time);
            client_update_accumulator = 0.0f;
        }

        renderer_clear_screen(vec4_create(0.3f, 0.5f, 0.9f, 1.0f));

        if (self_player.id != PLAYER_INVALID_ID) {
            vec2 username_position = vec2_create(
                self_player.position.x - (renderer_get_font_width(FA16) * strlen(username))/2,
                self_player.position.y + player_texture.height/2
            );
            renderer_draw_text(username, FA16, username_position, 1.0f, COLOR_MILK, 1.0f);
            renderer_draw_sprite(&player_texture, self_player.position, 1.0f, 0.0f);
        }

        /* Draw all other players */
        f32 server_update_accumulator_copy = server_update_accumulator;
        for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
            if (other_players[i].id != PLAYER_INVALID_ID) {
                vec2 position;
                if (!other_players[i].interp_complete) {
                    // Interpolate player's position based on the current and last position and time since last server update
                    f32 t = server_update_accumulator_copy / SERVER_TICK_DURATION;
                    if (t > 1.0f) {
                        t = 1.0f;
                        other_players[i].interp_complete = true;
                    }

                    vec2 player_position = {
                        .x = math_lerpf(other_players[i].last_position.x, other_players[i].position.x, t),
                        .y = math_lerpf(other_players[i].last_position.y, other_players[i].position.y, t)
                    };

                    position = player_position;
                } else {
                    position = other_players[i].position;
                }

                vec2 username_position = vec2_create(
                    position.x - (renderer_get_font_width(FA16) * strlen(other_players[i].name))/2,
                    position.y + player_texture.height/2
                );
                renderer_draw_text(other_players[i].name, FA16, username_position, 1.0f, COLOR_MILK, 1.0f);
                renderer_draw_sprite(&player_texture, position, 1.0f, 0.0f);
            }
        }

        display_build_version();

        chat_render();

        glfwPollEvents();
        glfwSwapBuffers(main_window);
    }

    LOG_INFO("client shutting down");

    /* Tell server to remove ourselves from the player list */
    packet_player_remove_t player_remove_packet = {
        .id = self_player.id
    };
    if (!packet_send(client_socket, PACKET_TYPE_PLAYER_REMOVE, &player_remove_packet)) {
        LOG_ERROR("failed to send player remove packet");
    }

    LOG_INFO("removed self from players");

    texture_destroy(&player_texture);

    event_system_unregister(EVENT_CODE_CHAR_PRESSED, chat_char_pressed_event_callback);
    event_system_unregister(EVENT_CODE_KEY_PRESSED, chat_key_pressed_event_callback);
    event_system_unregister(EVENT_CODE_KEY_REPEATED, chat_key_repeated_event_callback);

    event_system_unregister(EVENT_CODE_KEY_PRESSED, key_pressed_event_callback);
    event_system_unregister(EVENT_CODE_KEY_RELEASED, key_released_event_callback);

    event_system_unregister(EVENT_CODE_MOUSE_BUTTON_PRESSED, chat_mouse_button_pressed_event_callback);

    event_system_unregister(EVENT_CODE_WINDOW_CLOSED, window_closed_event_callback);
    event_system_unregister(EVENT_CODE_WINDOW_RESIZED, window_resized_event_callback);

    chat_shutdown();
    renderer_shutdown();
    event_system_shutdown();

    LOG_INFO("shutting down glfw");
    glfwDestroyWindow(main_window);
    glfwTerminate();

    pthread_kill(network_thread, SIGINT);
    pthread_join(network_thread, NULL);

    ring_buffer_destroy(ring_buffer);
    LOG_TRACE("destroyed ring buffer");

    return EXIT_SUCCESS;
}
