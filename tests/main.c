#include "test_manager.h"
#include "src/containers/stack_tests.h"
#include "src/containers/darray_tests.h"
#include "src/containers/ring_buffer_tests.h"

int main(void)
{
    test_manager_init();

    stack_register_tests();
    darray_register_tests();
    ring_buffer_register_tests();

    test_manager_run_all_tests();
    test_manager_shutdown();

    return 0;
}
