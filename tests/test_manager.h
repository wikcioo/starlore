#pragma once

#include "defines.h"

#define SKIP_RESULT 255

typedef b8 (*fp_test)(void);

void test_manager_init(void);
void test_manager_register_test(fp_test test, const char *description);
void test_manager_run_all_tests(void);
void test_manager_shutdown(void);
