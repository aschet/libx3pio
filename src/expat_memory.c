/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The memory of Expat, taken from the allocator of the parser that runs.
 */

#include "expat_memory.h"
#include "allocator.h"

/* Expat's functions for memory take no pointer of the program, so the allocator
 * of the parser that is running is kept per thread while Expat is called. Each
 * block remembers the allocator it came from, and so can be reallocated and
 * released in any thread. */
static X3PIO_THREAD_LOCAL const x3pio_allocator *current_allocator;

typedef union block_header
{
    struct
    {
        const x3pio_allocator *allocator;
        size_t size;
    } fields;
    max_align_t alignment;
} block_header;

static void *expat_malloc(size_t size)
{
    const x3pio_allocator *allocator = current_allocator;
    if (allocator == NULL || size > SIZE_MAX - sizeof(block_header))
    {
        return NULL;
    }
    block_header *header = x3pio_allocate(allocator, sizeof *header + size);
    if (header == NULL)
    {
        return NULL;
    }
    header->fields.allocator = allocator;
    header->fields.size = size;
    return header + 1;
}

static void expat_free(void *memory)
{
    if (memory != NULL)
    {
        block_header *header = (block_header *)memory - 1;
        x3pio_release(header->fields.allocator, header);
    }
}

static void *expat_realloc(void *memory, size_t size)
{
    if (memory == NULL)
    {
        return expat_malloc(size);
    }
    block_header *old = (block_header *)memory - 1;
    const x3pio_allocator *allocator = old->fields.allocator;
    const x3pio_allocator *saved = current_allocator;
    current_allocator = allocator;
    void *grown = expat_malloc(size);
    current_allocator = saved;
    if (grown == NULL)
    {
        return NULL;
    }
    memcpy(grown, memory, old->fields.size < size ? old->fields.size : size);
    expat_free(memory);
    return grown;
}

const XML_Memory_Handling_Suite *x3pio_expat_suite(void)
{
    static const XML_Memory_Handling_Suite suite = {expat_malloc, expat_realloc,
                                                    expat_free};
    return &suite;
}

const x3pio_allocator *x3pio_expat_enter(const x3pio_allocator *allocator)
{
    const x3pio_allocator *saved = current_allocator;
    current_allocator = allocator;
    return saved;
}

void x3pio_expat_leave(const x3pio_allocator *saved)
{
    current_allocator = saved;
}
