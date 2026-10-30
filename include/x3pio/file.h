/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Files by path, and the stream over a C file.
 * `<x3pio/x3pio.h>` does not include this header.
 *
 * A path is UTF-8, also on Windows, where the library converts it to the wide
 * characters of the file functions.
 */

#ifndef X3PIO_FILE_H
#define X3PIO_FILE_H

#include <x3pio/reader.h>
#include <x3pio/types.h>
#include <x3pio/writer.h>
#include <x3pio/x3pio_export.h>

#include <stdio.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Creates a stream over a C file. The file remains owned by the caller and is
 * not closed.
 *
 * The file must be open in binary mode. On Windows `stdin` and `stdout` are in
 * text mode, which translates line ends and treats the byte 0x1A as the end of
 * the input. Switch them before the first use with
 * `_setmode(_fileno(stdin), _O_BINARY)`. The files of x3pio_open_file() and
 * x3pio_create_file() are always binary.
 *
 * @param[out] stream The stream, whose `user` is the file.
 * @param[in] file The C file.
 */
X3PIO_API void x3pio_stdio_stream(x3pio_stream *stream, FILE *file);

/**
 * Opens the file at a path as the source and reads the description, as
 * x3pio_open_memory() does for memory. Unlike x3pio_open_stream() the function
 * does not read the whole file into memory: the reader reads the members that a
 * call needs. The file must remain unchanged while the reader is in use.
 *
 * @param[in,out] reader The reader.
 * @param[in] path The path of the file, in UTF-8.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null.
 * @retval X3PIO_ERR_IO The file cannot be opened.
 * @return Otherwise the result of x3pio_open_memory().
 */
X3PIO_API x3pio_status x3pio_open_file(x3pio_reader *reader, const char *path);

/**
 * Creates the file at a path and starts it as x3pio_create_stream() does. An
 * existing file is truncated. The container is written in place, and
 * x3pio_end_file() completes and closes the file. If the writing fails, the
 * data written so far remains. A document that cannot be written creates no
 * file.
 *
 * @param[in,out] writer The writer.
 * @param[in] path The path of the file, in UTF-8.
 * @param[in] document The description of the file.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null or the document cannot be
 * written.
 * @retval X3PIO_ERR_STATE The writer has created a file already.
 * @retval X3PIO_ERR_IO The file cannot be created.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
X3PIO_API x3pio_status x3pio_create_file(x3pio_writer *writer, const char *path,
                                         const x3pio_document *document);

#ifdef __cplusplus
}
#endif

#endif /* X3PIO_FILE_H */
