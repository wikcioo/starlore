#include "../../expect.h"
#include "../../test_manager.h"

#include "common/containers/ring_buffer.h"

b8 ring_buffer_create_and_destroy(void)
{
    void *ring_buffer = ring_buffer_create(sizeof(i32));
    expect_true(ring_buffer != 0);
    expect_equal(ring_buffer_capacity(ring_buffer), RING_BUFFER_DEFAULT_CAPACITY + 1);
    expect_equal(ring_buffer_stride(ring_buffer), sizeof(i32));
    expect_equal(ring_buffer_head(ring_buffer), 0);
    expect_equal(ring_buffer_tail(ring_buffer), 0);

    expect_equal(ring_buffer_length(ring_buffer), 0);
    expect_true(ring_buffer_is_empty(ring_buffer));

    ring_buffer_destroy(ring_buffer);
    expect_equal(((u64 *)ring_buffer)[RING_BUFFER_FIELD_CAPACITY], 0);
    expect_equal(((u64 *)ring_buffer)[RING_BUFFER_FIELD_STRIDE], 0);
    expect_equal(((u64 *)ring_buffer)[RING_BUFFER_FIELD_HEAD], 0);
    expect_equal(((u64 *)ring_buffer)[RING_BUFFER_FIELD_TAIL], 0);
    expect_equal(((u64 *)ring_buffer)[RING_BUFFER_FIELD_COUNT], 0);

    return true;
}

b8 ring_buffer_enqueue_and_dequeue(void)
{
    struct mydata {
        i32 foo;
        i64 bar;
    };

    u64 initial_capacity = 5;
    void *ring_buffer = ring_buffer_reserve(initial_capacity, sizeof(struct mydata));

    struct mydata md1 = { .foo = 1, .bar = 101 };
    struct mydata md2 = { .foo = 2, .bar = 102 };
    struct mydata md3 = { .foo = 3, .bar = 103 };
    struct mydata md4 = { .foo = 4, .bar = 104 };
    struct mydata md5 = { .foo = 5, .bar = 105 };
    struct mydata md6 = { .foo = 6, .bar = 106 };

    ring_buffer_enqueue(ring_buffer, md1, 0);
    expect_equal(ring_buffer_length(ring_buffer), 1);

    ring_buffer_enqueue(ring_buffer, md2, 0);
    ring_buffer_enqueue(ring_buffer, md3, 0);
    ring_buffer_enqueue(ring_buffer, md4, 0);
    ring_buffer_enqueue(ring_buffer, md5, 0);

    expect_equal(ring_buffer_length(ring_buffer), 5);
    expect_true(ring_buffer_is_full(ring_buffer));

    b8 enqueue_status;
    ring_buffer_enqueue(ring_buffer, md6, &enqueue_status);
    expect_false(enqueue_status);

    expect_equal(ring_buffer_length(ring_buffer), 5);

    struct mydata omd1 = {0};
    struct mydata omd2 = {0};
    struct mydata omd3 = {0};
    struct mydata omd4 = {0};
    struct mydata omd5 = {0};
    struct mydata omd6 = {0};

    ring_buffer_dequeue(ring_buffer, &omd1, 0);
    expect_equal(omd1.foo, 1);
    expect_equal(omd1.bar, 101);

    ring_buffer_dequeue(ring_buffer, &omd2, 0);
    expect_equal(omd2.foo, 2);
    expect_equal(omd2.bar, 102);

    ring_buffer_dequeue(ring_buffer, &omd3, 0);
    expect_equal(omd3.foo, 3);
    expect_equal(omd3.bar, 103);

    expect_equal(ring_buffer_length(ring_buffer), 2);

    ring_buffer_enqueue(ring_buffer, md6, 0);

    ring_buffer_dequeue(ring_buffer, &omd4, 0);
    expect_equal(omd4.foo, 4);
    expect_equal(omd4.bar, 104);

    ring_buffer_dequeue(ring_buffer, &omd5, 0);
    expect_equal(omd5.foo, 5);
    expect_equal(omd5.bar, 105);

    ring_buffer_dequeue(ring_buffer, &omd6, 0);
    expect_equal(omd6.foo, 6);
    expect_equal(omd6.bar, 106);

    expect_true(ring_buffer_is_empty(ring_buffer));

    ring_buffer_destroy(ring_buffer);

    return true;
}

void ring_buffer_register_tests(void)
{
    test_manager_register_test(ring_buffer_create_and_destroy, "ring buffer: create and destroy");
    test_manager_register_test(ring_buffer_enqueue_and_dequeue, "ring buffer: enqueue and dequeue");
}
