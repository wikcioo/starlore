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

#include "defines.h"
#include "shader.h"
#include "common/packet.h"
#include "common/logger.h"
#include "common/maths.h"

#define POLLFD_COUNT 2
#define INPUT_BUFFER_SIZE 1024
#define POLL_INFINITE_TIMEOUT -1

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

typedef struct player {
    player_id id;
    vec2 position;
    vec3 color;
} player_t;

static u32 other_player_count = 0;
static player_t other_players[MAX_PLAYER_COUNT];
static player_t self_player;
static struct pollfd pfds[POLLFD_COUNT];
static i32 client_socket;
static char input_buffer[INPUT_BUFFER_SIZE] = {0};
static u32 input_count = 0;
static char username[MAX_AUTHOR_SIZE];
static b8 running = false;
static vec2 current_window_size;

static shader_t flat_color_shader;
static mat4 ortho_projection;

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
        } else {
            packet_message_t message_packet = {0};

            u32 username_size = strlen(username) > MAX_AUTHOR_SIZE ? MAX_AUTHOR_SIZE : strlen(username);
            strncpy(message_packet.author, username, username_size);
            u32 content_size = input_count > MAX_CONTENT_SIZE ? MAX_CONTENT_SIZE : input_count;
            strncpy(message_packet.content, input_buffer, content_size);

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
    #define INPUT_BUFFER_SIZE 1024
    u8 recv_buffer[INPUT_BUFFER_SIZE] = {0};

    i64 bytes_read = recv(client_socket, &recv_buffer, INPUT_BUFFER_SIZE, 0);
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

    if (bytes_read < PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]) { /* Check if at least 'header' amount of bytes were read */
        LOG_WARN("unimplemented"); /* TODO: Handle the case of having read less than header size */
    } else {
        /* Check if multiple packets included in single tcp data reception */
        u8 *buffer = recv_buffer;
        for (;;) {
            packet_header_t *header = (packet_header_t *)buffer;

            if (bytes_read - PACKET_TYPE_SIZE[PACKET_TYPE_HEADER] < header->size) {
                LOG_WARN("unimplemented"); /* TODO: Handle the case of not having read the entire packet body */
            } else {
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
                    } break;
                    case PACKET_TYPE_PLAYER_ADD: {
                        received_data_size = PACKET_TYPE_SIZE[PACKET_TYPE_PLAYER_ADD];
                        packet_player_add_t *player_add = (packet_player_add_t *)(buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER]);

                        b8 found_free_slot = false;
                        for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                            if (other_players[i].id == PLAYER_INVALID_ID) { /* Free slot */
                                LOG_INFO("adding new player id=%u", player_add->id);
                                other_players[i].id       = player_add->id;
                                other_players[i].position = player_add->position;
                                other_players[i].color    = player_add->color;
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
                        b8 found_player_to_update = false;
                        for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
                            if (other_players[i].id == player_update->id) {
                                other_players[i].position = player_update->position;
                                found_player_to_update = true;
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

                buffer = (buffer + PACKET_TYPE_SIZE[PACKET_TYPE_HEADER] + received_data_size);
                packet_header_t *next_header = (packet_header_t *)buffer;
                if (next_header->type <= PACKET_TYPE_NONE || next_header->type >= PACKET_TYPE_COUNT) {
                    break;
                }
            }
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

void glfw_framebuffer_size_callback(GLFWwindow* window, i32 width, i32 height)
{
    current_window_size = vec2_create((f32)width, (f32)height);
    ortho_projection = mat4_orthographic(0.0f, current_window_size.x, 0.0f, current_window_size.y, -1.0f, 1.0f);

    shader_bind(&flat_color_shader);
    shader_set_uniform_mat4(&flat_color_shader, "u_projection", &ortho_projection);
    shader_unbind(&flat_color_shader);

    glViewport(0, 0, width, height);
}

void glfw_key_callback(GLFWwindow* window, i32 key, i32 scancode, i32 action, i32 mods)
{
    if (key == GLFW_KEY_P && action == GLFW_PRESS)
        send_ping_packet();

    // TODO: remove temporary code
    // NOTE: hardcoded velocity, doesn't consider delta time
    // Check for movement
    vec2 movement = vec2_zero();
    f32 velocity = 10.0f;
    if (key == GLFW_KEY_W) {  /* Up */
        movement.y += velocity;
    }
    if (key == GLFW_KEY_S) { /* Down */
        movement.y -= velocity;
    }
    if (key == GLFW_KEY_A) { /* Left */
        movement.x -= velocity;
    }
    if (key == GLFW_KEY_D) { /* Right */
        movement.x += velocity;
    }

    self_player.position = vec2_add(self_player.position, movement);

    packet_player_update_t player_update_packet = {
        .id       = self_player.id,
        .position = self_player.position
    };
    if (!packet_send(client_socket, PACKET_TYPE_PLAYER_UPDATE, &player_update_packet)) {
        LOG_ERROR("failed to send player update packet");
    }
}

void signal_handler(i32 sig)
{
    running = false;
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

    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Chat Client", NULL, NULL);
    if (window == NULL) {
        LOG_FATAL("failed to create glfw window");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    current_window_size = vec2_create((f32)WINDOW_WIDTH, (f32)WINDOW_HEIGHT);
    ortho_projection = mat4_orthographic(0.0f, current_window_size.x, 0.0f, current_window_size.y, -1.0f, 1.0f);

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);
    glfwSetKeyCallback(window, glfw_key_callback);

    glfwSwapInterval(1); // enable vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        LOG_FATAL("failed to load opengl");
        glfwDestroyWindow(window);
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    LOG_INFO("running opengl version %d.%d", GLVersion.major, GLVersion.minor);
    LOG_INFO("running glfw version %s", glfwGetVersionString());

    shader_create_info_t create_info;
    create_info.vertex_filepath = "assets/shaders/flat_color.vert";
    create_info.fragment_filepath = "assets/shaders/flat_color.frag";

    if (shader_create(&create_info, &flat_color_shader)) {
        LOG_INFO("compiled flat_color_shader");
    }
    shader_bind(&flat_color_shader);
    shader_set_uniform_mat4(&flat_color_shader, "u_projection", &ortho_projection);
    shader_unbind(&flat_color_shader);

    i32 tile_size = 32;
    f32 vertices[] = { // 32px x 32px square centered around centroid
        -tile_size / 2.0f, -tile_size / 2.0f,
        -tile_size / 2.0f,  tile_size / 2.0f,
         tile_size / 2.0f,  tile_size / 2.0f,
         tile_size / 2.0f, -tile_size / 2.0f,
    };

    u32 indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    u32 vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    u32 vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), (void *)vertices, GL_STATIC_DRAW);

    u32 ibo;
    glGenBuffers(1, &ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), (void *)indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(f32), 0);

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

    pthread_t network_thread;
    pthread_create(&network_thread, NULL, handle_networking, NULL);

    f32 rotation_angle = 0.0f;
    f32 scale_factor = 1.0f;

    f64 last_time = glfwGetTime();
    f64 delta_time = 0.0f;

    f32 time_accumulator = 0.0f;
    f32 fps_info_period = 5.0f; // 5 second

    while (!glfwWindowShouldClose(window) && running) {
        f64 now = glfwGetTime();
        delta_time = now - last_time;
        last_time = now;

        time_accumulator += delta_time;
        if (time_accumulator >= fps_info_period) {
            time_accumulator = 0.0f;
            LOG_INFO("running at %lf FPS", 1.0f / delta_time);
        }

        glClearColor(0.3f, 0.5f, 0.9f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        shader_bind(&flat_color_shader);

        glBindVertexArray(vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);

        if (self_player.id != PLAYER_INVALID_ID) {
            /* Draw ourselves */
            mat4 translation = mat4_translate(self_player.position);
            mat4 rotation = mat4_rotate(rotation_angle);
            mat4 scale = mat4_scale(vec2_create(scale_factor, scale_factor));
            mat4 model = mat4_multiply(translation, mat4_multiply(rotation, scale));

            shader_set_uniform_mat4(&flat_color_shader, "u_model", &model);
            shader_set_uniform_vec3(&flat_color_shader, "u_color", &self_player.color);

            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
        }

        /* Draw all other players */
        for (i32 i = 0; i < MAX_PLAYER_COUNT; i++) {
            if (other_players[i].id != PLAYER_INVALID_ID) {
                mat4 translation = mat4_translate(other_players[i].position);
                mat4 rotation = mat4_rotate(rotation_angle);
                mat4 scale = mat4_scale(vec2_create(scale_factor, scale_factor));
                mat4 model = mat4_multiply(translation, mat4_multiply(rotation, scale));

                shader_set_uniform_mat4(&flat_color_shader, "u_model", &model);
                shader_set_uniform_vec3(&flat_color_shader, "u_color", &other_players[i].color);

                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, NULL);
            }
        }

        glfwPollEvents();
        glfwSwapBuffers(window);
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

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

    shader_destroy(&flat_color_shader);

    LOG_INFO("shutting down glfw");
    glfwDestroyWindow(window);
    glfwTerminate();

    pthread_kill(network_thread, SIGINT);
    pthread_join(network_thread, NULL);

    return EXIT_SUCCESS;
}
