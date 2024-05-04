#pragma once

#include "common/logger.h"

#define expect_equal(actual, expected)                                                              \
    if (actual != expected) {                                                                       \
        LOG_ERROR("expected %lld, but got %lld in %s:%d", expected, actual, __FILE__, __LINE__);    \
        return false;                                                                               \
    }

#define expect_true(condition)                                                                      \
    if (!(condition)) {                                                                             \
        LOG_ERROR("expected condition %s to be true in %s:%d", #condition, __FILE__, __LINE__);     \
        return false;                                                                               \
    }

#define expect_false(condition)                                                                     \
    if (condition) {                                                                                \
        LOG_ERROR("expected condition %s to be false in %s:%d", #condition, __FILE__, __LINE__);    \
        return false;                                                                               \
    }
