#include "test_manager.h"

#include <stdio.h>

#include "common/containers/darray.h"

typedef struct test_instance {
    fp_test test_function;
    const char *description;
} test_instance_t;

static test_instance_t *test_instances;

void test_manager_init(void)
{
    test_instances = darray_create(sizeof(test_instance_t));
}

void test_manager_register_test(fp_test test, const char *description)
{
    test_instance_t inst = { .test_function = test, .description = description };
    darray_push(test_instances, inst);
}

void test_manager_run_all_tests(void)
{
    u32 passed = 0;
    u32 failed = 0;
    u32 skipped = 0;

    u64 length = darray_length(test_instances);
    for (i32 i = 0; i < length; i++) {
        printf("testing %s... ", test_instances[i].description);
        fflush(stdout);
        b8 result = test_instances[i].test_function();
        if (result == true) {
            printf(" passed\n");
            passed++;
        } else if (result == false) {
            // error message printed by test itself
            failed++;
        } else if (result == SKIP_RESULT) {
            printf(" skipped\n");
            skipped++;
        }
    }

    printf("summary: finished running %llu tests: %u passed, %u failed, %u skipped\n", length, passed, failed, skipped);
}

void test_manager_shutdown(void)
{
    darray_destroy(test_instances);
}
