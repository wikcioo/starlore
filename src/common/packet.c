#include "packet.h"

#include <errno.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/socket.h>

#include "common/net.h"
#include "common/asserts.h"
#include "common/logger.h"
#include "common/memory/memutils.h"

static u64 packet_sequence_number = 0;

b8 packet_send(i32 socket, u32 type, void *packet_data)
{
    ASSERT(type > PACKET_TYPE_NONE && type < PACKET_TYPE_COUNT);
    ASSERT(packet_data);

    packet_header_t header = {
        .type = type,
        .size = PACKET_TYPE_SIZE[type]
    };

    // TODO: Replace with arena allocator
    u32 buffer_size = sizeof(packet_header_t) + header.size;
    b8 *buffer = (b8 *)mem_alloc(buffer_size, MEMORY_TAG_NETWORK);

    mem_copy(buffer, (void *)&header, sizeof(packet_header_t));
    mem_copy(buffer + sizeof(packet_header_t), packet_data, header.size);

    // TODO: Change it to add data to the queue instead of sending it straightaway
    i64 bytes_sent_total = 0;
    i64 bytes_sent = 0;
    while (bytes_sent_total < buffer_size) {
        bytes_sent = net_send(socket, buffer + bytes_sent, buffer_size - bytes_sent, 0);
        if (bytes_sent == -1) {
            LOG_ERROR("packet_enqueue error: %s", strerror(errno));
            return false;
        }
        bytes_sent_total += bytes_sent;
    }

    mem_free(buffer, buffer_size, MEMORY_TAG_NETWORK);

    return bytes_sent_total == buffer_size;
}

u64 packet_get_next_sequence_number(void)
{
    return packet_sequence_number++;
}
