#include "../../expect.h"
#include "../../test_manager.h"

#include "common/memory/arena_allocator.h"

b8 arena_allocator_create_and_destroy(void)
{
    arena_allocator_t allocator;
    arena_allocator_create(sizeof(i32), 0, &allocator);

    expect_not_equal(allocator.memory, 0);
    expect_equal(allocator.total_size, sizeof(i32));
    expect_equal(allocator.current_offset, 0);
    expect_true(allocator.is_memory_owner);

    arena_allocator_destroy(&allocator);
    return true;
}

b8 arena_allocator_single_allocation_all_space(void)
{
    u64 arena_size = 10 * sizeof(u64);

    arena_allocator_t allocator;
    arena_allocator_create(arena_size, 0, &allocator);

    void *memory_block = arena_allocator_allocate(&allocator, arena_size);
    expect_not_equal(memory_block, 0);
    expect_equal(allocator.current_offset, arena_size);

    arena_allocator_destroy(&allocator);
    return true;
}

b8 arena_allocator_multi_allocation_all_space(void)
{
    u64 num_allocations = 100;

    arena_allocator_t allocator;
    arena_allocator_create(num_allocations * sizeof(u64), 0, &allocator);

    void *memory_block;
    for (i32 i = 0; i < num_allocations; i++) {
        memory_block = arena_allocator_allocate(&allocator, sizeof(u64));
        expect_not_equal(memory_block, 0);
        expect_equal(allocator.current_offset, (i+1) * sizeof(u64));
    }

    arena_allocator_destroy(&allocator);
    return true;

}

b8 arena_allocator_over_allocate(void)
{
    u64 arena_size = 10 * sizeof(u64);

    arena_allocator_t allocator;
    arena_allocator_create(arena_size, 0, &allocator);

    void *memory_block;

    memory_block = arena_allocator_allocate(&allocator, arena_size);
    expect_not_equal(memory_block, 0);
    expect_equal(allocator.current_offset, arena_size);

    LOG_INFO("\n>>> the following error is intentional");
    memory_block = arena_allocator_allocate(&allocator, sizeof(u64));
    expect_equal(memory_block, 0);
    expect_equal(allocator.current_offset, arena_size);

    arena_allocator_destroy(&allocator);
    return true;
}

b8 arena_allocator_allocate_all_space_and_free_all(void)
{
    u64 arena_size = 10 * sizeof(u64);

    arena_allocator_t allocator;
    arena_allocator_create(arena_size, 0, &allocator);

    void *memory_block = arena_allocator_allocate(&allocator, arena_size);
    expect_not_equal(memory_block, 0);
    expect_equal(allocator.current_offset, arena_size);

    arena_allocator_free_all(&allocator);
    expect_equal(allocator.current_offset, 0);

    arena_allocator_destroy(&allocator);
    return true;
}

void arena_allocator_register_tests(void)
{
    test_manager_register_test(arena_allocator_create_and_destroy, "arena allocator: create and destroy");
    test_manager_register_test(arena_allocator_single_allocation_all_space, "arena allocator: single allocation all space");
    test_manager_register_test(arena_allocator_multi_allocation_all_space, "arena allocator: multi allocation all space");
    test_manager_register_test(arena_allocator_over_allocate, "arena allocator: over allocate");
    test_manager_register_test(arena_allocator_allocate_all_space_and_free_all, "arena allocator: allocate all space and free all space");
}
