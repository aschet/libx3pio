/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The zip container, on top of minizip-ng.
 *
 * No other file of the library includes a header of minizip-ng, so this is the
 * seam to replace it.
 */

#ifndef X3PIO_SRC_ZIP_H
#define X3PIO_SRC_ZIP_H

#include "buffer.h"
#include "common.h"

/** Where a zip file is: on disk, or in memory that stays where it is. */
typedef struct x3pio_zip_source
{
    /** The path in UTF-8, or null for memory. */
    const char *path;
    /** The bytes of a zip file in memory. */
    const unsigned char *data;
    /** The number of bytes. */
    size_t size;
} x3pio_zip_source;

/**
 * Lists the stored names of the files of a zip file, folders left out.
 *
 * @param[in] source The zip file.
 * @param allocator The allocator of the names.
 * @param[out] names Receives the names, which the caller releases with
 * x3pio_zip_names_free().
 * @param[out] count Receives the number of names.
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_IO The file cannot be opened.
 * @retval X3PIO_ERR_FORMAT It is not a zip file.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
x3pio_status x3pio_zip_names(const x3pio_zip_source *source,
                             const x3pio_allocator *allocator, char ***names,
                             size_t *count, const char **message);

/**
 * Releases the names of x3pio_zip_names().
 *
 * @param allocator The allocator that was given.
 * @param[in,out] names The names.
 * @param count The number of names.
 */
void x3pio_zip_names_free(const x3pio_allocator *allocator, char **names,
                          size_t count);

/** An open member of a zip file, read from the start to the end. */
typedef struct x3pio_zip_entry x3pio_zip_entry;

/**
 * Opens a member of a zip file for reading.
 *
 * @param[in] source The zip file.
 * @param allocator The allocator of the entry.
 * @param stored_name The name as the zip file stores it.
 * @param[out] entry Receives the entry, closed with x3pio_zip_entry_close().
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_IO The file cannot be opened, or has no such member.
 * @retval X3PIO_ERR_FORMAT The zip file or the member is damaged.
 * @retval X3PIO_ERR_UNSUPPORTED The member is encrypted or uses a method that
 * is not supported.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
x3pio_status x3pio_zip_entry_open(const x3pio_zip_source *source,
                                  const x3pio_allocator *allocator,
                                  const char *stored_name,
                                  x3pio_zip_entry **entry,
                                  const char **message);

/**
 * Returns the size of the member after decompression, as the zip file states.
 *
 * @param[in] entry The entry.
 */
uint64_t x3pio_zip_entry_size(const x3pio_zip_entry *entry);

/**
 * Reads the next bytes of a member.
 *
 * @param[in,out] entry The entry.
 * @param[out] buffer Receives the bytes.
 * @param size The size of the buffer.
 * @param[out] got Receives the number of bytes, 0 at the end.
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_FORMAT The member is damaged, or its checksum is wrong.
 */
x3pio_status x3pio_zip_entry_read(x3pio_zip_entry *entry, void *buffer,
                                  size_t size, size_t *got,
                                  const char **message);

/**
 * Closes a member. A null entry is ignored. The checksum that the zip file
 * states for the member is not checked: x3pio_zip_entry_finish() does it.
 *
 * @param[in,out] entry The entry.
 */
void x3pio_zip_entry_close(x3pio_zip_entry *entry);

/**
 * Reads the end of a member that has been read completely, checks the checksum
 * that the zip file states for it, and closes it. A null entry is ignored.
 *
 * @param[in,out] entry The entry.
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_FORMAT The member is damaged, or longer than the zip file
 * states, or its checksum is wrong.
 */
x3pio_status x3pio_zip_entry_finish(x3pio_zip_entry *entry,
                                    const char **message);

/**
 * Reads a whole member into a buffer, at most `limit` bytes.
 *
 * @param[in] source The zip file.
 * @param allocator The allocator of the buffer.
 * @param stored_name The name as the zip file stores it.
 * @param limit The most bytes to read; more is a damaged file.
 * @param[out] out Receives the bytes, released with x3pio_buffer_free().
 * @param[out] message Receives the reason of a failure.
 * @return The errors of x3pio_zip_entry_open() and x3pio_zip_entry_read(), and
 * X3PIO_ERR_FORMAT if the member is larger than the limit.
 */
x3pio_status x3pio_zip_read_member(const x3pio_zip_source *source,
                                   const x3pio_allocator *allocator,
                                   const char *stored_name, size_t limit,
                                   x3pio_buffer *out, const char **message);

/** A zip file being written. */
typedef struct x3pio_zip_writer x3pio_zip_writer;

/**
 * Starts a zip file on disk. An existing file is truncated.
 *
 * @param allocator The allocator of the writer.
 * @param path The path in UTF-8.
 * @param[out] writer Receives the writer.
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_IO The file cannot be created.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
x3pio_status x3pio_zip_writer_open_file(const x3pio_allocator *allocator,
                                        const char *path,
                                        x3pio_zip_writer **writer,
                                        const char **message);

/**
 * Starts a zip file in memory.
 *
 * @param allocator The allocator of the writer.
 * @param[in,out] out The buffer that receives the file, emptied first. It has
 * to stay alive until the writer is freed.
 * @param[out] writer Receives the writer.
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
x3pio_status x3pio_zip_writer_open_memory(const x3pio_allocator *allocator,
                                          x3pio_buffer *out,
                                          x3pio_zip_writer **writer,
                                          const char **message);

/**
 * Starts a member. The previous one must be finished.
 *
 * @param[in,out] writer The writer.
 * @param name The name of the member.
 * @param[in] compression The compression.
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_IO The member cannot be started.
 */
x3pio_status x3pio_zip_writer_begin(x3pio_zip_writer *writer, const char *name,
                                    const x3pio_compression *compression,
                                    const char **message);

/**
 * Writes the next bytes of the member that is started.
 *
 * @param[in,out] writer The writer.
 * @param data The bytes.
 * @param size The number of bytes.
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_IO The bytes cannot be written.
 */
x3pio_status x3pio_zip_writer_write(x3pio_zip_writer *writer, const void *data,
                                    size_t size, const char **message);

/**
 * Completes the member that is started.
 *
 * @param[in,out] writer The writer.
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_IO The member cannot be completed.
 */
x3pio_status x3pio_zip_writer_end(x3pio_zip_writer *writer,
                                  const char **message);

/**
 * Writes the directory of the zip file and closes it. After a success the file
 * on disk is complete.
 *
 * @param[in,out] writer The writer.
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_IO The zip file cannot be completed.
 */
x3pio_status x3pio_zip_writer_close(x3pio_zip_writer *writer,
                                    const char **message);

/**
 * Releases a writer. A zip file that was not closed stays incomplete. A null
 * writer is ignored.
 *
 * @param[in,out] writer The writer.
 */
void x3pio_zip_writer_free(x3pio_zip_writer *writer);

#endif /* X3PIO_SRC_ZIP_H */
