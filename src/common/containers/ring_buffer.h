#pragma once

#include "defines.h"

/********************************************************************************
 *  Memory layout of ring buffer:                                               *
 *    u64 capacity - max amount of elements that can be stored without resizing *
 *    u64 stride   - size of each element in bytes                              *
 *    u64 head     - index of the head element                                  *
 *    u64 tail     - index of the tail element                                  *
 *    void *data   - start of actual data                                       *
 ********************************************************************************/

#define RING_BUFFER_DEFAULT_CAPACITY 10

enum ring_buffer_field {
    RING_BUFFER_FIELD_CAPACITY,
    RING_BUFFER_FIELD_STRIDE,
    RING_BUFFER_FIELD_HEAD,
    RING_BUFFER_FIELD_TAIL,
    RING_BUFFER_FIELD_COUNT
};

void *_ring_buffer_create(u64 capacity, u64 stride);
void  _ring_buffer_destroy(void *ring_buffer);

void _ring_buffer_enqueue(void *ring_buffer, const void *element, b8 *status);
void _ring_buffer_dequeue(void *ring_buffer, void *out_element, b8 *status);

b8 _ring_buffer_is_full(void *ring_buffer);
b8 _ring_buffer_is_empty(void *ring_buffer);

u64 _ring_buffer_length(void *ring_buffer);

b8 _ring_buffer_peek_from_end(void *ring_buffer, u64 n, void *out_element);

u64 _ring_buffer_field_get(void *ring_buffer, u64 field);
void _ring_buffer_field_set(void *ring_buffer, u64 field, u64 value);


#define ring_buffer_create(stride) \
    _ring_buffer_create(RING_BUFFER_DEFAULT_CAPACITY, stride)

#define ring_buffer_reserve(capacity, stride) \
    _ring_buffer_create(capacity, stride)

#define ring_buffer_destroy(ring_buffer) \
    _ring_buffer_destroy(ring_buffer)

#define ring_buffer_enqueue(ring_buffer, element, status)   \
    {                                                       \
        typeof(element) tmp = element;                      \
        _ring_buffer_enqueue(ring_buffer, &tmp, status);    \
    }

#define ring_buffer_dequeue(ring_buffer, out_element, status) \
    _ring_buffer_dequeue(ring_buffer, out_element, status)

#define ring_buffer_is_full(ring_buffer) \
    _ring_buffer_is_full(ring_buffer)

#define ring_buffer_is_empty(ring_buffer) \
    _ring_buffer_is_empty(ring_buffer)

#define ring_buffer_peek_from_end(ring_buffer, n, out_element) \
    _ring_buffer_peek_from_end(ring_buffer, n, out_element)

#define ring_buffer_capacity(ring_buffer) \
    _ring_buffer_field_get(ring_buffer, RING_BUFFER_FIELD_CAPACITY)

#define ring_buffer_length(ring_buffer) \
    _ring_buffer_length(ring_buffer)

#define ring_buffer_stride(ring_buffer) \
    _ring_buffer_field_get(ring_buffer, RING_BUFFER_FIELD_STRIDE)

#define ring_buffer_head(ring_buffer) \
    _ring_buffer_field_get(ring_buffer, RING_BUFFER_FIELD_HEAD)

#define ring_buffer_tail(ring_buffer) \
    _ring_buffer_field_get(ring_buffer, RING_BUFFER_FIELD_TAIL)
