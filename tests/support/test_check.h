/* Minimal assertion helper usable from C and C++ until a test framework ADR is accepted. */
#ifndef FIAISDK_TEST_CHECK_H
#define FIAISDK_TEST_CHECK_H

#include <stdio.h>

static int fiaisdk_test_failures = 0;

#define CHECK(cond)                                                                                \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond);               \
            ++fiaisdk_test_failures;                                                               \
        }                                                                                          \
    } while (0)

#define TEST_RESULT() (fiaisdk_test_failures == 0 ? 0 : 1)

#endif /* FIAISDK_TEST_CHECK_H */
