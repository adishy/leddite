#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

// Shared assertion macros for the native unit tests.
//
// Extracted from test_text_renderer.cpp, which defined them inline. The older
// test files still carry their own copies; new tests include this instead.

#include <stdio.h>

static int testsPassed = 0;
static int testsFailed = 0;

#define ASSERT(cond, msg)                                            \
    do {                                                             \
        if (!(cond)) {                                               \
            fprintf(stderr, "\n    FAIL: %s (line %d)\n", msg, __LINE__); \
            testsFailed++;                                           \
        } else {                                                     \
            testsPassed++;                                           \
        }                                                            \
    } while (0)

#define ASSERT_EQ(actual, expected, msg)                             \
    do {                                                             \
        const long _a = (long)(actual), _e = (long)(expected);       \
        if (_a != _e) {                                              \
            fprintf(stderr, "\n    FAIL: %s — got %ld, want %ld (line %d)\n", \
                    msg, _a, _e, __LINE__);                          \
            testsFailed++;                                           \
        } else {                                                     \
            testsPassed++;                                           \
        }                                                            \
    } while (0)

#define TEST(name) do { printf("  %-42s ", name); fflush(stdout); } while (0)
#define PASS()     printf("OK\n")

#define SUMMARY(suite)                                               \
    do {                                                             \
        printf("\n%s: %d passed, %d failed\n", suite, testsPassed, testsFailed); \
        return testsFailed > 0 ? 1 : 0;                              \
    } while (0)

#endif // TEST_HARNESS_H
