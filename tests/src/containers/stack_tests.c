#include "../../expect.h"
#include "../../test_manager.h"

#include "common/containers/stack.h"

b8 stack_create_and_destroy(void)
{
    stack_t stack;
    stack_create(sizeof(u64), &stack);

    u64 element = 0;
    b8 result = stack_pop(&stack, &element);
    expect_false(result);

    stack_destroy(&stack);
    return true;
}

b8 stack_peek_push_pop_clear(void)
{
    stack_t stack;
    stack_create(sizeof(u64), &stack);

    u64 element = 0;
    u64 peek_value = 0;
    u64 out_element = 0;

    element = 10;
    stack_push(&stack, &element);

    element = 20;
    stack_push(&stack, &element);

    stack_peek(&stack, &peek_value);
    expect_equal(peek_value, 20);

    stack_pop(&stack, &out_element);
    expect_equal(out_element, 20);

    stack_pop(&stack, &out_element);
    expect_equal(out_element, 10);

    element = 30;
    stack_push(&stack, &element);
    stack_push(&stack, &element);
    stack_push(&stack, &element);
    stack_clear(&stack);

    b8 result = stack_pop(&stack, &out_element);
    expect_false(result);

    stack_destroy(&stack);
    return true;
}

void stack_register_tests(void)
{
    test_manager_register_test(stack_create_and_destroy, "stack: create and destroy");
    test_manager_register_test(stack_peek_push_pop_clear, "stack: peek push pop clear");
}
