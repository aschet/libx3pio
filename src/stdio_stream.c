/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The stream over a C file.
 */

#include <x3pio/file.h>

#include "common.h"

static size_t stdio_read(void *user, void *data, size_t size)
{
    FILE *file = user;
    const size_t count = fread(data, 1, size, file);
    if (count < size && ferror(file) != 0)
    {
        return X3PIO_STREAM_ERROR;
    }
    return count;
}

static size_t stdio_write(void *user, const void *data, size_t size)
{
    return fwrite(data, 1, size, (FILE *)user);
}

void x3pio_stdio_stream(x3pio_stream *stream, FILE *file)
{
    x3pio_stream_init(stream);
    stream->user = file;
    stream->read = stdio_read;
    stream->write = stdio_write;
}
