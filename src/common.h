/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Helpers that every source file of the library uses.
 */

#ifndef X3PIO_SRC_COMMON_H
#define X3PIO_SRC_COMMON_H

#include <x3pio/convert.h>
#include <x3pio/x3pio.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * Memory management callbacks that every allocation of the library goes
 * through. They are internal: the tests give a handle an allocator that fails
 * on demand with the `_with` functions of the create functions, and a program
 * has the default one, malloc() and free(). The zip libraries and the system
 * libraries allocate with malloc() whatever the callbacks are.
 */
typedef struct x3pio_allocator
{
    /** Opaque pointer passed as the first argument of both callbacks. */
    void *user;
    /**
     * Returns a block of at least `size` bytes, aligned for any object type,
     * or null on failure.
     */
    void *(*allocate)(void *user, size_t size);
    /** Releases a block returned by `allocate`. Never called with null. */
    void (*release)(void *user, void *memory);
} x3pio_allocator;

/** Storage that every thread has a copy of, as C11 has it. */
#if defined(_MSC_VER) && !defined(__clang__) && _MSC_VER < 1928
#define X3PIO_THREAD_LOCAL __declspec(thread)
#else
#define X3PIO_THREAD_LOCAL _Thread_local
#endif

/** The number of points a reader or a writer converts at a time. */
#define X3PIO_DEFAULT_CHUNK 8192
/** The largest number of bytes a conversion needs per point. */
#define X3PIO_BYTES_PER_POINT 64

/**
 * Tells whether a number of points is a valid size of a chunk: not 0, and not
 * so large that the buffers of a chunk cannot be counted in bytes.
 *
 * @param chunk The number of points.
 */
static inline bool x3pio_chunk_valid(size_t chunk)
{
    return chunk > 0 && chunk <= SIZE_MAX / X3PIO_BYTES_PER_POINT;
}

/**
 * Limits a number to the range of the elements of a rotation, [-1, 1].
 *
 * @param value The number.
 */
static inline double x3pio_clamp_unit(double value)
{
    if (value < -1.0)
    {
        return -1.0;
    }
    return value > 1.0 ? 1.0 : value;
}

/**
 * Multiplies two sizes.
 *
 * @param a The first factor.
 * @param b The second factor.
 * @param[out] product Receives the product.
 * @return False if it does not fit a size_t.
 */
static inline bool x3pio_multiply(size_t a, size_t b, size_t *product)
{
    if (a != 0 && b > SIZE_MAX / a)
    {
        return false;
    }
    *product = a * b;
    return true;
}

/**
 * Adds two sizes.
 *
 * @param a The first summand.
 * @param b The second summand.
 * @param[out] sum Receives the sum.
 * @return False if it does not fit a size_t.
 */
static inline bool x3pio_add(size_t a, size_t b, size_t *sum)
{
    if (b > SIZE_MAX - a)
    {
        return false;
    }
    *sum = a + b;
    return true;
}

/**
 * Records the text of a failure and returns its code. The text is a string that
 * stays valid for the life of the program, so a handle holds no memory for it.
 *
 * @param[out] message Receives the text. May be null.
 * @param code The status to return.
 * @param[in] text The text, a string that stays valid.
 * @return `code`.
 */
static inline x3pio_status x3pio_fail(const char **message, x3pio_status code,
                                      const char *text)
{
    if (message != NULL)
    {
        *message = text;
    }
    return code;
}

#endif /* X3PIO_SRC_COMMON_H */
