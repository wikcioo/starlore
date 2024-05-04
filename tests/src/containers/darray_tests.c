#include "../../expect.h"
#include "../../test_manager.h"

#include "common/containers/darray.h"

b8 darray_create_and_destroy(void)
{
    void *array = darray_create(sizeof(i32));
    expect_true(array != 0);
    expect_equal(darray_capacity(array), DARRAY_DEFAULT_CAPACITY);
    expect_equal(darray_length(array), 0);
    expect_equal(darray_stride(array), sizeof(i32));

    darray_destroy(array);
    expect_equal(((u64 *)array)[DARRAY_FIELD_CAPACITY], 0);
    expect_equal(((u64 *)array)[DARRAY_FIELD_LENGTH], 0);
    expect_equal(((u64 *)array)[DARRAY_FIELD_STRIDE], 0);
    expect_equal(((u64 *)array)[DARRAY_FIELD_COUNT], 0);

    return true;
}

b8 darray_push_and_pop(void)
{
    struct mydata {
        i32 foo;
        i64 bar;
    };

    u64 initial_capacity = 3;
    void *array = darray_reserve(initial_capacity, sizeof(struct mydata));

    struct mydata md1 = { .foo = 1, .bar = 101 };
    struct mydata md2 = { .foo = 2, .bar = 102 };
    struct mydata md3 = { .foo = 3, .bar = 103 };
    struct mydata md4 = { .foo = 4, .bar = 104 };

    darray_push(array, md1);
    darray_push(array, md2);
    darray_push(array, md3);
    expect_equal(darray_capacity(array), initial_capacity);

    darray_push(array, md4);
    expect_equal(darray_capacity(array), initial_capacity * DARRAY_DEFAULT_RESIZE_FACTOR);

    struct mydata omd1 = {0};
    struct mydata omd2 = {0};
    struct mydata omd3 = {0};
    struct mydata omd4 = {0};

    darray_pop(array, &omd1);
    darray_pop(array, &omd2);
    darray_pop(array, &omd3);
    darray_pop(array, &omd4);

    expect_equal(omd1.foo, 4);
    expect_equal(omd1.bar, 104);
    expect_equal(omd2.foo, 3);
    expect_equal(omd2.bar, 103);
    expect_equal(omd3.foo, 2);
    expect_equal(omd3.bar, 102);
    expect_equal(omd4.foo, 1);
    expect_equal(omd4.bar, 101);

    expect_equal(darray_length(array), 0);

    darray_destroy(array);

    return true;
}

void darray_register_tests(void)
{
    test_manager_register_test(darray_create_and_destroy, "darray: create and destroy");
    test_manager_register_test(darray_push_and_pop, "darray: push and pop");
}
