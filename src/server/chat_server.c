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

#include "defines.h"
#include "common/packet.h"

#define SERVER_BACKLOG 10

typedef struct server_pfd
{
    struct pollfd *fds;
    u32 count;
    u32 capacity;
} server_pfd_t;

static i32 server_socket;
static server_pfd_t fds;

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
            fprintf(stderr, "failed to resize pollfd array\n");
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

    printf("error: did not find fd=%d in the array of pfds\n", fd);
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
        perror("validation: send error");
        return false;
    } else if (bytes_sent != sizeof(puzzle_value)) {
        fprintf(stderr, "validation: failed to send %lu bytes of validation data\n", sizeof(puzzle_value));
        return false;
    }

    u64 answer = puzzle_value ^ 0xDEADBEEFCAFEBABE; /* TODO: Come up with a better validation function */

    u64 result_buffer;
    bytes_read = recv(client_socket, (void *)&result_buffer, sizeof(result_buffer), 0);
    if (bytes_read <= 0) {
        if (bytes_read == -1) {
            perror("validation: recv error");
        } else if (bytes_read == 0) {
            fprintf(stderr, "validation: orderly shutdown\n");
            /* Socket closed by the handler */
        }
        return false;
    }

    if (bytes_read == sizeof(result_buffer)) {
        b8 status_buffer = result_buffer == answer;
        bytes_sent = send(client_socket, (void *)&status_buffer, sizeof(status_buffer), 0);
        if (bytes_sent == -1) {
            perror("validation status: send error");
            return false;
        } else if (bytes_sent != sizeof(status_buffer)) {
            fprintf(stderr, "validation status: failed to send %lu bytes of validation data\n", sizeof(puzzle_value));
            return false;
        }

        return status_buffer;
    }

    fprintf(stderr, "validation: received incorrect number of bytes\n");
    return false;
}

void handle_new_connection_request_event(void)
{
    struct sockaddr_storage client_addr;
    u32 client_addr_len = sizeof(client_addr);

    i32 client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_socket == -1) {
        perror("accept error");
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

    printf("new connection from %s:%hu\n", client_ip, port);

    if (!validate_incoming_client(client_socket)) {
        fprintf(stderr, "%s:%hu failed validation\n", client_ip, port);
        close(client_socket);
        return;
    }

    printf("%s:%hu passed validation\n", client_ip, port);
    server_pfd_add(&fds, client_socket);
}

void handle_client_event(i32 client_socket)
{
    /* Read all functionality */
    #define INPUT_BUFFER_SIZE 1024
    u8 input_buffer[INPUT_BUFFER_SIZE] = {0};

    i64 bytes_read = recv(client_socket, &input_buffer, INPUT_BUFFER_SIZE, 0);
    if (bytes_read <= 0) {
        if (bytes_read == -1) {
            perror("recv error");
        } else if (bytes_read == 0) {
            fprintf(stderr, "orderly shutdown\n");
            server_pfd_remove(&fds, client_socket);
            close(client_socket);
        }
        return;
    }

    if (bytes_read < sizeof(packet_header_t)) { /* Check if at least 'header' amount of bytes were read */
        fprintf(stderr, "unimplemented\n"); /* TODO: Handle the case of having read less than header size */
    } else {
        packet_header_t *header = (packet_header_t *)input_buffer;
        if (bytes_read - sizeof(packet_header_t) < header->size) { 
            fprintf(stderr, "unimplemented\n"); /* TODO: Handle the case of not having read the entire packet body */
        } else {
            switch (header->type) {
                case PACKET_TYPE_NONE: {
                    fprintf(stderr, "received PACKET_TYPE_NONE, ignoring...\n");
                } break;
                case PACKET_TYPE_PING: { /* Bounce back the packet */
                    i64 bytes_sent = send(client_socket, input_buffer, sizeof(packet_header_t) + sizeof(packet_ping_t), 0);
                    if (bytes_sent == -1) {
                        perror("ping send error");
                    }
                } break;
                case PACKET_TYPE_MESSAGE: {
                    packet_message_t *message = (packet_message_t *)(input_buffer + sizeof(header));
                    printf("author: %s, content: %s\n", message->author, message->content);

                    time_t current_time = time(NULL);
                    message->timestamp = (i64)current_time;

                    i32 bytes_sent;
                    u32 packet_size = sizeof(packet_header_t) + sizeof(packet_message_t);
                    for (i32 i = 0; i < fds.count; i++) {
                        if (/*fds.fds[i].fd == client_socket || */fds.fds[i].fd == server_socket) {
                            continue;
                        }
                        
                        bytes_sent = send(fds.fds[i].fd, input_buffer, packet_size, 0);
                        if (bytes_sent == -1) {
                            perror("message send error");
                        } else if (bytes_sent < packet_size) {
                            fprintf(stderr, "message failed to send all %u bytes\n", packet_size);
                        }
                    }
                } break;
                default:
                    fprintf(stderr, "received unknown packet type, ignoring...\n");
            }
        }
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct addrinfo hints;
    struct addrinfo *result, *rp;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM; /* Stream socket */
    hints.ai_family = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    hints.ai_flags = AI_PASSIVE;     /* For wildcard IP addresses */

    i32 status = getaddrinfo(NULL, argv[1], &hints, &result);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        server_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (server_socket == -1) {
            continue;
        }

        static i32 yes = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (void *)&yes, sizeof(i32)) == -1) {
            perror("setsockopt reuse address error");
        }

        if (bind(server_socket, rp->ai_addr, rp->ai_addrlen) == 0) {
            break; /* Success */
        }

        close(server_socket);
    }

    freeaddrinfo(result);

    if (rp == NULL) { /* No address succeeded */
        fprintf(stderr, "could not bind\n");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, SERVER_BACKLOG) == -1) {
        perror("failed to start listening");
        exit(EXIT_FAILURE);
    }

    server_pfd_init(5, &fds);
    server_pfd_add(&fds, server_socket);

    for (;;) {
        i32 num_events = poll(fds.fds, fds.count, -1); /* Wait indefinately */

        if (num_events == -1) {
            perror("poll error");
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

    server_pfd_shutdown(&fds);

    return EXIT_SUCCESS;
}
