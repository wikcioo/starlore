#include "ring_buffer.h"

#include <memory.h>
#include <stdlib.h>

#include "common/asserts.h"
#include "common/memory/memutils.h"

#define HEADER_SIZE (RING_BUFFER_FIELD_COUNT * sizeof(u64))

void *_ring_buffer_create(u64 capacity, u64 stride)
{
    ASSERT(capacity > 0);
    ASSERT(stride > 0);

    u64 total_size = HEADER_SIZE + capacity * stride;
    u64 *memory = (u64 *)mem_alloc(total_size, MEMORY_TAG_RING_BUFFER);

    memory[RING_BUFFER_FIELD_CAPACITY] = capacity + 1;
    memory[RING_BUFFER_FIELD_STRIDE]   = stride;
    memory[RING_BUFFER_FIELD_HEAD]     = 0;
    memory[RING_BUFFER_FIELD_TAIL]     = 0;

    return (void *)&memory[RING_BUFFER_FIELD_COUNT];
}

void _ring_buffer_destroy(void *ring_buffer)
{
    ASSERT(ring_buffer);

    u64 capacity = ring_buffer_capacity(ring_buffer);
    u64 stride = ring_buffer_stride(ring_buffer);

    _ring_buffer_field_set(ring_buffer, RING_BUFFER_FIELD_CAPACITY, 0);
    _ring_buffer_field_set(ring_buffer, RING_BUFFER_FIELD_STRIDE, 0);
    _ring_buffer_field_set(ring_buffer, RING_BUFFER_FIELD_HEAD, 0);
    _ring_buffer_field_set(ring_buffer, RING_BUFFER_FIELD_TAIL, 0);

    u64 *header = (u64 *)ring_buffer - RING_BUFFER_FIELD_COUNT;
    mem_free(header, HEADER_SIZE + capacity * stride, MEMORY_TAG_RING_BUFFER);
}

void _ring_buffer_enqueue(void *ring_buffer, const void *element, b8 *status)
{
    ASSERT(ring_buffer);
    ASSERT(element);

    if (ring_buffer_is_full(ring_buffer)) {
        if (status) {
            *status = false;
        }
        return;
    }

    u64 capacity = ring_buffer_capacity(ring_buffer);
    u64 stride = ring_buffer_stride(ring_buffer);
    u64 head = ring_buffer_head(ring_buffer);

    mem_copy((u8 *)ring_buffer + (head * stride), element, stride);
    _ring_buffer_field_set(ring_buffer, RING_BUFFER_FIELD_HEAD, (head + 1) % (capacity - 1));
    if (status) {
        *status = true;
    }
}

void _ring_buffer_dequeue(void *ring_buffer, void *out_element, b8 *status)
{
    ASSERT(ring_buffer);

    if (ring_buffer_is_empty(ring_buffer)) {
        if (status) {
            *status = false;
        }
        return;
    }

    u64 capacity = ring_buffer_capacity(ring_buffer);
    u64 stride = ring_buffer_stride(ring_buffer);
    u64 tail = ring_buffer_tail(ring_buffer);

    mem_copy(out_element, (u8 *)ring_buffer + (tail * stride), stride);
    _ring_buffer_field_set(ring_buffer, RING_BUFFER_FIELD_TAIL, (tail + 1) % (capacity - 1));
    if (status) {
        *status = true;
    }
}

b8 _ring_buffer_is_full(void *ring_buffer)
{
    ASSERT(ring_buffer);

    u64 capacity = ring_buffer_capacity(ring_buffer);
    u64 head = ring_buffer_head(ring_buffer);
    u64 tail = ring_buffer_tail(ring_buffer);

    return (tail - head + capacity) % capacity == 1;
}

b8 _ring_buffer_is_empty(void *ring_buffer)
{
    ASSERT(ring_buffer);

    u64 head = ring_buffer_head(ring_buffer);
    u64 tail = ring_buffer_tail(ring_buffer);

    return head == tail;
}

u64 _ring_buffer_length(void *ring_buffer)
{
    ASSERT(ring_buffer);

    u64 capacity = ring_buffer_capacity(ring_buffer);
    u64 head = ring_buffer_head(ring_buffer);
    u64 tail = ring_buffer_tail(ring_buffer);

    return (head - tail) % capacity;
}

b8 _ring_buffer_peek_from_end(void *ring_buffer, u64 n, void *out_element)
{
    ASSERT(ring_buffer);
    ASSERT(n >= 0)
    ASSERT(out_element);

    u64 capacity = ring_buffer_capacity(ring_buffer);
    u64 stride = ring_buffer_stride(ring_buffer);
    u64 head = ring_buffer_head(ring_buffer);
    u64 tail = ring_buffer_tail(ring_buffer);

    u64 element_index = (tail + n) % capacity;
    if ((element_index < tail || element_index >= head) || (element_index > capacity - 1)) {
        return false;
    }

    mem_copy(out_element, (u8 *)ring_buffer + (element_index * stride), stride);
    return true;
}

u64 _ring_buffer_field_get(void *ring_buffer, u64 field)
{
    ASSERT(ring_buffer);
    ASSERT(field <= RING_BUFFER_FIELD_COUNT);

    u64 *header = (u64 *)ring_buffer - RING_BUFFER_FIELD_COUNT;
    return header[field];
}

void _ring_buffer_field_set(void *ring_buffer, u64 field, u64 value)
{
    ASSERT(ring_buffer);
    ASSERT(field < RING_BUFFER_FIELD_COUNT);

    u64 *header = (u64 *)ring_buffer - RING_BUFFER_FIELD_COUNT;
    header[field] = value;
}
