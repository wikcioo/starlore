#include "darray.h"

#include <memory.h>
#include <stdlib.h>

#include "common/asserts.h"
#include "common/memory/memutils.h"

#define HEADER_SIZE (DARRAY_FIELD_COUNT * sizeof(u64))

void *_darray_create(u64 capacity, u64 stride)
{
    ASSERT(capacity > 0);
    ASSERT(stride > 0);

    u64 total_size = HEADER_SIZE + capacity * stride;
    u64 *memory = (u64 *)mem_alloc(total_size, MEMORY_TAG_DARRAY);

    memory[DARRAY_FIELD_CAPACITY] = capacity;
    memory[DARRAY_FIELD_STRIDE]   = stride;
    memory[DARRAY_FIELD_LENGTH]   = 0;

    return (void *)&memory[DARRAY_FIELD_COUNT];
}

void _darray_destroy(void *array)
{
    ASSERT(array);

    u64 capacity = darray_capacity(array);
    u64 stride = darray_stride(array);

    _darray_field_set(array, DARRAY_FIELD_CAPACITY, 0);
    _darray_field_set(array, DARRAY_FIELD_STRIDE, 0);
    _darray_field_set(array, DARRAY_FIELD_LENGTH, 0);

    u64 *header = (u64 *)array - DARRAY_FIELD_COUNT;
    mem_free(header, HEADER_SIZE + capacity * stride, MEMORY_TAG_DARRAY);
}

void *_darray_resize(void *array, u64 new_capacity)
{
    ASSERT(array);

    u64 capacity = darray_capacity(array);
    u64 length = darray_length(array);
    u64 stride = darray_stride(array);

    ASSERT(new_capacity >= capacity);
    UNUSED(capacity); // prevents compiler warning in release mode

    void *new_array = _darray_create(new_capacity, stride);
    mem_copy(new_array, array, length * stride);

    _darray_field_set(new_array, DARRAY_FIELD_LENGTH, length);
    _darray_destroy(array);
    return new_array;
}

void *_darray_push(void *array, const void *element)
{
    ASSERT(array);
    ASSERT(element);

    u64 capacity = darray_capacity(array);
    u64 length = darray_length(array);
    u64 stride = darray_stride(array);

    if (length >= capacity) {
        array = _darray_resize(array, capacity * DARRAY_DEFAULT_RESIZE_FACTOR);
    }

    mem_copy((u8 *)array + (length * stride), element, stride);
    _darray_field_set(array, DARRAY_FIELD_LENGTH, length + 1);
    return array;
}

void _darray_pop(void *array, void *out_element)
{
    ASSERT(array);

    u64 *header = (u64 *)array - DARRAY_FIELD_COUNT;
    u64 length = darray_length(array);
    u64 stride = darray_stride(array);

    ASSERT(length > 0);

    if (out_element) {
        u8 *data = (u8 *)&header[DARRAY_FIELD_COUNT];
        mem_copy(out_element, data + ((length - 1) * stride), stride);
    }

    _darray_field_set(array, DARRAY_FIELD_LENGTH, length - 1);
}

void *_darray_push_at(void *array, u64 index, const void *element)
{
    ASSERT(array);
    ASSERT(element);

    u64 capacity = darray_capacity(array);
    u64 length = darray_length(array);
    u64 stride = darray_stride(array);

    ASSERT(index >= 0 && index < length);

    if (length >= capacity) {
        array = _darray_resize(array, capacity * DARRAY_DEFAULT_RESIZE_FACTOR);
    }

    mem_copy((u8 *)array + ((index + 1) * stride), (u8 *)array + (index * stride), (length - index) * stride);
    mem_copy((u8 *)array + (index * stride), element, stride);
    _darray_field_set(array, DARRAY_FIELD_LENGTH, length + 1);
    return array;
}

void _darray_pop_at(void *array, u64 index, void *out_element)
{
    ASSERT(array);

    u64 *header = (u64 *)array - DARRAY_FIELD_COUNT;
    u64 length = darray_length(array);
    u64 stride = darray_stride(array);

    ASSERT(length > 0);
    ASSERT(index >= 0 && index < length);

    if (out_element) {
        u8 *data = (u8 *)&header[DARRAY_FIELD_COUNT];
        mem_copy(out_element, data + (index * stride), stride);
    }

    mem_copy((u8 *)array + (index * stride), (u8 *)array + ((index + 1) * stride), (length - index - 1) * stride);
    _darray_field_set(array, DARRAY_FIELD_LENGTH, length - 1);
}

u64 _darray_field_get(void *array, u64 field)
{
    ASSERT(array);
    ASSERT(field <= DARRAY_FIELD_COUNT);

    u64 *header = (u64 *)array - DARRAY_FIELD_COUNT;
    return header[field];
}

void _darray_field_set(void *array, u64 field, u64 value)
{
    ASSERT(array);
    ASSERT(field < DARRAY_FIELD_COUNT);

    u64 *header = (u64 *)array - DARRAY_FIELD_COUNT;
    header[field] = value;
}
