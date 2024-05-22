#pragma once

#include "defines.h"

i64 net_send(i32 socket, const void *buffer, u64 size, i32 flags);
i64 net_recv(i32 socket, void *buffer, u64 size, i32 flags);
void net_get_bandwidth(u64 *up, u64 *down);
void net_update(f64 delta_time);
