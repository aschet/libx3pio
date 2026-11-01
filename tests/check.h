/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* A small harness: a failed check is counted and reported, and the test goes
 * on. The program exits with the number of failures. */

#ifndef X3PIO_TESTS_CHECK_H
#define X3PIO_TESTS_CHECK_H

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <x3pio/x3pio.h>

static int failures = 0;
static int checks = 0;

#define CHECK(condition)                                                       \
    do                                                                         \
    {                                                                          \
        ++checks;                                                              \
        if (!(condition))                                                      \
        {                                                                      \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__,   \
                    #condition);                                               \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

#define CHECK_STATUS(expected, call)                                           \
    do                                                                         \
    {                                                                          \
        const x3pio_status actual_status = (call);                             \
        ++checks;                                                              \
        if (actual_status != (expected))                                       \
        {                                                                      \
            fprintf(stderr, "%s:%d: %s returned \"%s\", expected \"%s\"\n",    \
                    __FILE__, __LINE__, #call,                                 \
                    x3pio_status_string(actual_status),                        \
                    x3pio_status_string(expected));                            \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

#define CHECK_OK(call) CHECK_STATUS(X3PIO_OK, call)

/* Compares two numbers as doubles. Where the processor computes in extended
 * precision (the x87 unit of 32-bit x86), a literal such as 0.1 in a comparison
 * may keep more digits than a double holds, and the double that was read or
 * computed differs from it. The arguments of a call are doubles. */
static inline bool equals(double a, double b)
{
    return a == b;
}

/* Compares two numbers that may be NaN, which equal each other here. */
#define CHECK_SAME(a, b)                                                       \
    do                                                                         \
    {                                                                          \
        const double same_a = (a);                                             \
        const double same_b = (b);                                             \
        ++checks;                                                              \
        if (!((isnan(same_a) && isnan(same_b)) || same_a == same_b))           \
        {                                                                      \
            fprintf(stderr, "%s:%d: %s = %.17g, expected %.17g\n", __FILE__,   \
                    __LINE__, #a, same_a, same_b);                             \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

#define CHECK_NEAR(a, b, tolerance)                                            \
    do                                                                         \
    {                                                                          \
        const double near_a = (a);                                             \
        const double near_b = (b);                                             \
        ++checks;                                                              \
        if (!(fabs(near_a - near_b) <= (tolerance)))                           \
        {                                                                      \
            fprintf(stderr, "%s:%d: %s = %.17g, expected %.17g\n", __FILE__,   \
                    __LINE__, #a, near_a, near_b);                             \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

#define CHECK_STRING(a, b)                                                     \
    do                                                                         \
    {                                                                          \
        const char *string_a = (a);                                            \
        const char *string_b = (b);                                            \
        ++checks;                                                              \
        if (string_a == NULL || string_b == NULL                               \
            || strcmp(string_a, string_b) != 0)                                \
        {                                                                      \
            fprintf(stderr, "%s:%d: %s = \"%s\", expected \"%s\"\n", __FILE__, \
                    __LINE__, #a, string_a != NULL ? string_a : "(null)",      \
                    string_b != NULL ? string_b : "(null)");                   \
            ++failures;                                                        \
        }                                                                      \
    } while (0)

static inline int finish_tests(void)
{
    if (failures != 0)
    {
        fprintf(stderr, "%d of %d checks failed\n", failures, checks);
        return EXIT_FAILURE;
    }
    printf("%d checks passed\n", checks);
    return EXIT_SUCCESS;
}

#endif /* X3PIO_TESTS_CHECK_H */
