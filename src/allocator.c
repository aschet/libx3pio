/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Allocation and release of memory through the allocator of the program.
 */

#include "allocator.h"

static void *default_allocate(void *user, size_t size)
{
    (void)user;
    return malloc(size);
}

static void default_release(void *user, void *memory)
{
    (void)user;
    free(memory);
}

const x3pio_allocator *x3pio_default_allocator(void)
{
    static const x3pio_allocator allocator = {NULL, default_allocate,
                                              default_release};
    return &allocator;
}

void *x3pio_allocate(const x3pio_allocator *allocator, size_t size)
{
    return allocator->allocate(allocator->user, size != 0 ? size : 1);
}

void *x3pio_allocate_array(const x3pio_allocator *allocator, size_t count,
                           size_t size)
{
    size_t bytes = 0;
    if (!x3pio_multiply(count, size, &bytes))
    {
        return NULL;
    }
    return x3pio_allocate(allocator, bytes);
}

void *x3pio_allocate_zeroed(const x3pio_allocator *allocator, size_t count,
                            size_t size)
{
    size_t bytes = 0;
    if (!x3pio_multiply(count, size, &bytes))
    {
        return NULL;
    }
    void *memory = x3pio_allocate(allocator, bytes);
    if (memory != NULL)
    {
        memset(memory, 0, bytes != 0 ? bytes : 1);
    }
    return memory;
}

char **x3pio_allocate_strings(const x3pio_allocator *allocator, size_t count)
{
    return (char **)x3pio_allocate_array(allocator, count, sizeof(char *));
}

void x3pio_release_strings(const x3pio_allocator *allocator, char **strings)
{
    x3pio_release(allocator, (void *)strings);
}

void x3pio_copy_strings(char **dst, char *const *src, size_t count)
{
    if (count > 0)
    {
        memcpy((void *)dst, (const void *)src, count * sizeof(char *));
    }
}

void x3pio_release(const x3pio_allocator *allocator, void *memory)
{
    if (memory != NULL)
    {
        allocator->release(allocator->user, memory);
    }
}

bool x3pio_allocator_take(const x3pio_allocator *given, x3pio_allocator *used)
{
    if (given == NULL)
    {
        given = x3pio_default_allocator();
    }
    if (given->allocate == NULL || given->release == NULL)
    {
        return false;
    }
    *used = *given;
    return true;
}
