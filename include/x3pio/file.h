/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Files by path.
 * `<x3pio/x3pio.h>` does not include this header.
 *
 * A path is UTF-8, also on Windows, where the library converts it to the wide
 * characters of the file functions. A program on Windows that has its paths in
 * wide characters uses the variants x3pio_open_wfile() and
 * x3pio_create_wfile(), which take UTF-16.
 */

#ifndef X3PIO_FILE_H
#define X3PIO_FILE_H

#include <x3pio/reader.h>
#include <x3pio/types.h>
#include <x3pio/writer.h>
#include <x3pio/x3pio_export.h>

#include <wchar.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Opens the file at a path as the source and reads the document, as
 * x3pio_open_memory() does for memory. The function does not read the whole
 * file into memory: the reader reads the members that a call needs. The file
 * must remain unchanged while the reader is in use.
 *
 * @param[in,out] reader The reader.
 * @param[in] path The path of the file, in UTF-8.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null.
 * @retval X3PIO_ERR_IO The file cannot be opened.
 * @return Otherwise the result of x3pio_open_memory().
 */
X3PIO_API x3pio_status x3pio_open_file(x3pio_reader *reader, const char *path);

/**
 * Creates the file at a path and starts it as x3pio_create_memory() does for
 * memory. An
 * existing file is truncated. The container is written in place, and
 * x3pio_end_file() completes and closes the file. If the writing fails, the
 * data written so far remains. A document that cannot be written creates no
 * file.
 *
 * @param[in,out] writer The writer.
 * @param[in] path The path of the file, in UTF-8.
 * @param[in] document The document of the file.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null or the document cannot be
 * written.
 * @retval X3PIO_ERR_STATE The writer has created a file already.
 * @retval X3PIO_ERR_IO The file cannot be created.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
X3PIO_API x3pio_status x3pio_create_file(x3pio_writer *writer, const char *path,
                                         const x3pio_document *document);

#if defined(_WIN32)
/**
 * Variant of x3pio_open_file() with a path of wide characters, UTF-16 as
 * Windows has them.
 *
 * @param[in,out] reader The reader.
 * @param[in] path The path of the file.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, or the path has a surrogate
 * without a partner.
 * @return Otherwise the result of x3pio_open_file().
 */
X3PIO_API x3pio_status x3pio_open_wfile(x3pio_reader *reader,
                                        const wchar_t *path);

/**
 * Variant of x3pio_create_file() with a path of wide characters, UTF-16 as
 * Windows has them.
 *
 * @param[in,out] writer The writer.
 * @param[in] path The path of the file.
 * @param[in] document The document of the file.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, the document cannot be
 * written, or the path has a surrogate without a partner.
 * @return Otherwise the result of x3pio_create_file().
 */
X3PIO_API x3pio_status x3pio_create_wfile(x3pio_writer *writer,
                                          const wchar_t *path,
                                          const x3pio_document *document);
#endif

#ifdef __cplusplus
}
#endif

#endif /* X3PIO_FILE_H */
