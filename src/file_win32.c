/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Opening and creating of files by wide-character path on Windows.
 *
 * The path of a file is UTF-8 in the library, and minizip-ng converts it to the
 * wide characters of the functions of Windows. The functions here convert the
 * other way, from UTF-16, and call those that take a path in UTF-8. A path with
 * a surrogate that has no partner is no text in UTF-8, so it is refused.
 */

#include <x3pio/file.h>

#include "allocator.h"
#include "reader.h"
#include "writer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <limits.h>

/* The UTF-8 of a path of wide characters, released with the allocator. */
static x3pio_status utf8_of(const x3pio_allocator *allocator,
                            const wchar_t *path, const char **message,
                            char **utf8)
{
    *utf8 = NULL;
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path,
                                         -1, NULL, 0, NULL, NULL);
    if (size <= 0)
    {
        return x3pio_fail(message, X3PIO_ERR_ARGUMENT,
                          "The path is not valid UTF-16");
    }
    char *out = x3pio_allocate(allocator, (size_t)size);
    if (out == NULL)
    {
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, path, -1, out, size,
                            NULL, NULL)
        != size)
    {
        x3pio_release(allocator, out);
        return x3pio_fail(message, X3PIO_ERR_ARGUMENT,
                          "The path is not valid UTF-16");
    }
    *utf8 = out;
    return X3PIO_OK;
}

x3pio_status x3pio_open_wfile(x3pio_reader *reader, const wchar_t *path)
{
    if (reader == NULL || path == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    char *utf8 = NULL;
    x3pio_status status =
        utf8_of(&reader->allocator, path, &reader->message, &utf8);
    if (status != X3PIO_OK)
    {
        return status;
    }
    /* The reader keeps a copy of the path. */
    status = x3pio_open_file(reader, utf8);
    x3pio_release(&reader->allocator, utf8);
    return status;
}

x3pio_status x3pio_create_wfile(x3pio_writer *writer, const wchar_t *path,
                                const x3pio_document *document)
{
    if (writer == NULL || path == NULL || document == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    char *utf8 = NULL;
    x3pio_status status =
        utf8_of(&writer->allocator, path, &writer->message, &utf8);
    if (status != X3PIO_OK)
    {
        return status;
    }
    /* The file is open when the call returns. */
    status = x3pio_create_file(writer, utf8, document);
    x3pio_release(&writer->allocator, utf8);
    return status;
}
