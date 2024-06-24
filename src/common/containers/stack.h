#pragma once

#include "defines.h"

#define STACK_DEFAULT_CAPACITY  10
#define STACK_SIZE_LIMIT        MiB(1)

typedef struct {
    void *memory;
} stack_t;

void stack_create(u64 stride, stack_t *out_stack);
void stack_destroy(stack_t *stack);

b8   stack_peek (stack_t *stack, void *out_element);
b8   stack_push (stack_t *stack, void *element);
b8   stack_pop  (stack_t *stack, void *out_element);
void stack_clear(stack_t *stack);
