/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief A seekable stream in memory for minizip-ng. The stream of minizip-ng
 * itself counts in 32 bits, which limits a zip file in memory to 2 GiB; this
 * one counts in 64 bits, reads the bytes of the program where they are and
 * writes into a buffer of the library.
 */

#ifndef X3PIO_SRC_MEMORY_STREAM_H
#define X3PIO_SRC_MEMORY_STREAM_H

#include "buffer.h"
#include "common.h"

/**
 * Creates an open stream that reads from memory.
 *
 * @param[in] data The bytes, which have to stay where they are.
 * @param size The number of bytes.
 * @return The stream as minizip-ng takes it, or null on failure. Release it
 * with x3pio_memory_stream_free().
 */
void *x3pio_memory_stream_reader(const void *data, size_t size);

/**
 * Creates an open stream that writes into a buffer, which is emptied first.
 *
 * @param[in,out] out The buffer, which has to stay alive.
 * @return The stream as minizip-ng takes it, or null on failure. Release it
 * with x3pio_memory_stream_free().
 */
void *x3pio_memory_stream_writer(x3pio_buffer *out);

/**
 * Closes and releases a stream.
 *
 * @param[in,out] stream The stream. A null stream is ignored.
 */
void x3pio_memory_stream_free(void *stream);

#endif /* X3PIO_SRC_MEMORY_STREAM_H */
