/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief A buffer of bytes that grows.
 */

#ifndef X3PIO_SRC_BUFFER_H
#define X3PIO_SRC_BUFFER_H

#include "allocator.h"
#include "common.h"

/** A buffer of bytes that grows as it is appended to. */
typedef struct x3pio_buffer
{
    /** The allocator, which outlives the buffer. */
    const x3pio_allocator *allocator;
    /** The bytes, or null while it is empty. */
    unsigned char *data;
    /** The number of bytes in use. */
    size_t size;
    /** The number of bytes allocated. */
    size_t capacity;
    /** Whether an allocation of the buffer failed. */
    bool failed;
} x3pio_buffer;

/**
 * Starts an empty buffer.
 *
 * @param[out] buffer The buffer.
 * @param allocator The allocator, which has to outlive the buffer.
 */
void x3pio_buffer_init(x3pio_buffer *buffer, const x3pio_allocator *allocator);

/**
 * Releases the memory of a buffer and empties it.
 *
 * @param buffer The buffer.
 */
void x3pio_buffer_free(x3pio_buffer *buffer);

/**
 * Makes room for more bytes.
 *
 * @param buffer The buffer.
 * @param extra The number of bytes to add to the ones in use.
 * @return False if there is no memory.
 */
bool x3pio_buffer_reserve(x3pio_buffer *buffer, size_t extra);

/**
 * Appends bytes.
 *
 * @param buffer The buffer.
 * @param data The bytes. May be null if the size is 0.
 * @param size The number of bytes.
 * @return False if there is no memory.
 */
bool x3pio_buffer_append(x3pio_buffer *buffer, const void *data, size_t size);

/**
 * Appends a text without its terminator.
 *
 * @param buffer The buffer.
 * @param text The text.
 * @return False if there is no memory.
 */
bool x3pio_buffer_append_text(x3pio_buffer *buffer, const char *text);

/**
 * Appends a byte.
 *
 * @param buffer The buffer.
 * @param c The byte.
 * @return False if there is no memory.
 */
bool x3pio_buffer_append_char(x3pio_buffer *buffer, char c);

#endif /* X3PIO_SRC_BUFFER_H */
