#include "stack.h"

#include <memory.h>

#include "common/logger.h"
#include "common/asserts.h"
#include "common/containers/darray.h"

void stack_create(u64 stride, stack_t *out_stack)
{
    ASSERT(out_stack);

    memset(out_stack, 0, sizeof(stack_t));
    out_stack->memory = darray_reserve(STACK_DEFAULT_CAPACITY, stride);
}

void stack_destroy(stack_t *stack)
{
    ASSERT(stack && stack->memory);
    darray_destroy(stack->memory);
}

b8 stack_peek(stack_t *stack, void *out_element)
{
    ASSERT(stack && stack->memory);
    ASSERT(out_element);

    u64 element_count = darray_length(stack->memory);
    if (element_count < 1) {
        return false;
    }

    u64 stride = darray_stride(stack->memory);
    memcpy(out_element, (u8 *)stack->memory + (element_count-1) * stride, stride);
    return true;
}

b8 stack_push(stack_t *stack, void *element)
{
    ASSERT(stack && stack->memory);
    ASSERT(element);

    u64 element_count = darray_length(stack->memory);
    u64 element_size = darray_stride(stack->memory);
    if ((element_count + 1) * element_size > STACK_SIZE_LIMIT) {
        LOG_WARN("reached stack size limit of %u bytes", STACK_SIZE_LIMIT);
        return false;
    }

    _darray_push(stack->memory, element);
    return true;
}

b8 stack_pop(stack_t *stack, void *out_element)
{
    ASSERT(stack && stack->memory);

    u64 element_count = darray_length(stack->memory);
    if (element_count < 1) {
        return false;
    }

    darray_pop(stack->memory, out_element);
    return true;
}

void stack_clear(stack_t *stack)
{
    ASSERT(stack && stack->memory);
    darray_clear(stack->memory);
}
