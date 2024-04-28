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

#include "defines.h"
#include "common/packet.h"

#define POLLFD_COUNT 2
#define INPUT_BUFFER_SIZE 1024

static i32 client_socket;
static char input_buffer[INPUT_BUFFER_SIZE] = {0};
static u32 input_count = 0;
static char username[MAX_AUTHOR_SIZE];

b8 handle_client_validation(i32 client)
{
    u64 puzzle_buffer;
    i64 bytes_read, bytes_sent;

    bytes_read = recv(client, (void *)&puzzle_buffer, sizeof(puzzle_buffer), 0); /* TODO: Handle unresponsive server */
    if (bytes_read <= 0) {
        if (bytes_read == -1) {
            perror("validation: recv error");
        } else if (bytes_read == 0) {
            fprintf(stderr, "validation: orderly shutdown\n");
        }
        return false;
    }

    if (bytes_read == sizeof(puzzle_buffer)) {
        u64 answer = puzzle_buffer ^ 0xDEADBEEFCAFEBABE; /* TODO: Come up with a better validation function */
        bytes_sent = send(client, (void *)&answer, sizeof(answer), 0);
        if (bytes_sent == -1) {
            perror("validation: send error");
            return false;
        } else if (bytes_sent != sizeof(answer)) {
            fprintf(stderr, "validation: failed to send %lu bytes of validation data\n", sizeof(answer));
            return false;
        }

        b8 status_buffer;
        bytes_read = recv(client, (void *)&status_buffer, sizeof(status_buffer), 0);
        if (bytes_read <= 0) {
            if (bytes_read == -1) {
                perror("validation status: recv error");
            } else if (bytes_read == 0) {
                fprintf(stderr, "validation status: orderly shutdown\n");
            }
            return false;
        }

        return status_buffer;
    }

    fprintf(stderr, "validation: received incorrect number of bytes\n");
    return false;
}

void handle_stdin_event(void)
{
    char ch;
    if (read(STDIN_FILENO, &ch, 1) == -1) {
        perror("stdin read");
        return;
    }

    if (ch >= 32 && ch < 127) { /* Printable characters */
        input_buffer[input_count++] = ch;
    } else if (ch == '\n') { /* Send packet over the socket based on input buffer */
        if (input_buffer[0] == '/') { /* Handle special commands */
            if (strcmp(&input_buffer[1], "ping") == 0) {
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                u64 time_now = (u64)(ts.tv_sec * 1000000000 + ts.tv_nsec);

                packet_header_t header = { .type = PACKET_TYPE_PING, .size = sizeof(packet_ping_t) };
                packet_ping_t ping = { .time = time_now };

                u32 buffer_size = sizeof(header) + sizeof(ping);
                b8 *buffer = (b8 *)malloc(buffer_size); /* TODO: Replace with throwaway memory allocator*/
                memset(buffer, 0, buffer_size);

                memcpy(buffer, &header, sizeof(header));
                memcpy(buffer + sizeof(header), &ping, sizeof(ping));

                i64 bytes_sent = send(client_socket, buffer, buffer_size, 0);
                if (bytes_sent == -1) {
                    perror("ping send error");
                }

                free(buffer);
            } else if (strcmp(&input_buffer[1], "quit") == 0) {
                close(client_socket);
                exit(EXIT_SUCCESS);
            } else {
                fprintf(stderr, "unknown command\n");
            }

            input_count = 0;
            memset(input_buffer, 0, INPUT_BUFFER_SIZE);
        } else {
            packet_header_t header = { .type = PACKET_TYPE_MESSAGE, .size = sizeof(packet_message_t) };
            packet_message_t message = {0};

            memcpy(message.author, username, strlen(username));
            memcpy(message.content, input_buffer, input_count);

            u32 buffer_size = sizeof(header) + sizeof(message);
            b8 *buffer = (b8 *)malloc(buffer_size); /* TODO: Replace with throwaway memory allocator*/
            memset(buffer, 0, buffer_size);

            memcpy(buffer, &header, sizeof(header));
            memcpy(buffer + sizeof(header), &message, sizeof(message));

            i64 bytes_sent = send(client_socket, buffer, buffer_size, 0);
            if (bytes_sent == -1) {
                perror("ping send error");
            }

            free(buffer);

            input_count = 0;
            memset(input_buffer, 0, INPUT_BUFFER_SIZE);
        }
    }
}

void handle_socket_event(void)
{
    #define INPUT_BUFFER_SIZE 1024
    u8 buffer[INPUT_BUFFER_SIZE] = {0};

    i64 bytes_read = recv(client_socket, &buffer, INPUT_BUFFER_SIZE, 0);
    if (bytes_read <= 0) {
        if (bytes_read == -1) {
            perror("recv error");
        } else if (bytes_read == 0) {
            fprintf(stderr, "orderly shutdown\n");
            close(client_socket);
            exit(EXIT_FAILURE);
        }
        return;
    }

    if (bytes_read < sizeof(packet_header_t)) { /* Check if at least 'header' amount of bytes were read */
        fprintf(stderr, "unimplemented\n"); /* TODO: Handle the case of having read less than header size */
    } else {
        packet_header_t *header = (packet_header_t *)buffer;
        if (bytes_read - sizeof(packet_header_t) < header->size) { 
            fprintf(stderr, "unimplemented\n"); /* TODO: Handle the case of not having read the entire packet body */
        } else {
            switch (header->type) {
                case PACKET_TYPE_NONE: {
                    fprintf(stderr, "received PACKET_TYPE_NONE, ignoring...\n");
                } break;
                case PACKET_TYPE_PING: {
                    packet_ping_t *data = (packet_ping_t *)(buffer + sizeof(packet_header_t));

                    struct timespec ts;
                    clock_gettime(CLOCK_MONOTONIC, &ts);
                    u64 time_now = (u64)(ts.tv_sec * 1000000000 + ts.tv_nsec);

                    f64 ping_ms = (time_now - data->time) / 1000000.0;
                    printf("ping = %fms\n", ping_ms);
                } break;
                case PACKET_TYPE_MESSAGE: {
                    packet_message_t *message = (packet_message_t *)(buffer + sizeof(packet_header_t));
                    struct tm *local_time = localtime((time_t *)&message->timestamp);
                    printf("[%d-%02d-%02d %02d:%02d] %s: %s\n",
                           local_time->tm_year + 1900,
                           local_time->tm_mon + 1,
                           local_time->tm_mday,
                           local_time->tm_hour,
                           local_time->tm_min,
                           message->author,
                           message->content);
                } break;
                default:
                    fprintf(stderr, "received unknown packet type, ignoring...\n");
            }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s ip port username\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    memcpy(username, argv[3], strlen(argv[3]));

    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    i32 status_code = getaddrinfo(argv[1], argv[2], &hints, &result);
    if (status_code != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status_code));
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        client_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (client_socket == -1) {
            continue;
        }

        if (connect(client_socket, rp->ai_addr, rp->ai_addrlen) == -1) {
            perror("connect error"); /* TODO: Provide more information about failed parameters */
            continue;
        }

        break;
    }

    freeaddrinfo(result);

    if (rp == NULL) {
        fprintf(stderr, "failed to connect\n");
        exit(EXIT_FAILURE);
    }

    printf("connected to server at %s:%s\n", argv[1], argv[2]);

    struct pollfd pfds[POLLFD_COUNT];
    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;

    pfds[1].fd = client_socket;
    pfds[1].events = POLLIN;

    if (!handle_client_validation(client_socket)) {
        fprintf(stderr, "failed client validation\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    puts("client successfully validated");

    for (;;) {
        i32 num_events = poll(pfds, POLLFD_COUNT, -1);

        if (num_events == -1) {
            perror("poll event");
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

    return EXIT_SUCCESS;
}
