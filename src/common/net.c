#include "net.h"

#include <sys/socket.h>

#define STAT_UPDATE_PERIOD 1.0f

static f32 accumulator;

static u64 bytes_per_sec_up;
static u64 bytes_per_sec_down;
static u64 last_bytes_per_sec_up;
static u64 last_bytes_per_sec_down;

i64 net_send(i32 socket, const void *buffer, u64 size, i32 flags)
{
    i64 bytes_sent = send(socket, buffer, size, flags);
    if (bytes_sent > 0) {
        bytes_per_sec_up += bytes_sent;
    }

    return bytes_sent;
}

i64 net_recv(i32 socket, void *buffer, u64 size, i32 flags)
{
    i64 bytes_read = recv(socket, buffer, size, flags);
    if (bytes_read > 0) {
        bytes_per_sec_down += bytes_read;
    }

    return bytes_read;
}

void net_get_bandwidth(u64 *up, u64 *down)
{
    *up = last_bytes_per_sec_up;
    *down = last_bytes_per_sec_down;
}

void net_update(f64 delta_time)
{
    accumulator += delta_time;
    if (accumulator >= STAT_UPDATE_PERIOD) {
        last_bytes_per_sec_up = bytes_per_sec_up;
        last_bytes_per_sec_down = bytes_per_sec_down;
        bytes_per_sec_up = 0;
        bytes_per_sec_down = 0;
        accumulator = 0.0f;
    }
}
