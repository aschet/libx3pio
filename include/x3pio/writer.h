/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Writer of a file or of memory.
 *
 * @par Process
 * A writer writes one file at a time, from the start to the end:
 * -# x3pio_writer_create() creates the writer, and x3pio_writer_set_storage(),
 *    x3pio_writer_set_compression() and x3pio_writer_set_chunk() set the
 *    options.
 * -# x3pio_create_file() (in `<x3pio/file.h>`) or x3pio_create_memory() starts
 *    the file from a document, which fixes the layout of the points.
 * -# x3pio_write_heights() writes the whole matrix of heights, or
 *    x3pio_write_points() or x3pio_write_data() writes the rows of points in
 *    metres or as the file stores them, in parts of one row or more, in the
 *    order of the file, until all rows of the document are written.
 * -# x3pio_add_vendor_file() adds files of vendors, which is optional.
 * -# x3pio_end_file() writes main.xml and the checksum, and completes the
 *    container. A file is complete only after it.
 * -# x3pio_writer_free() releases the writer.
 *
 * @par States
 * A writer is in one of four states. It is new until a file is created, then
 * created. x3pio_end_file() moves it to ended, from created if all rows are
 * written. An error that loses the position in the file moves it to failed: an
 * error of the file, and a value in a call that writes that
 * cannot be written.
 * - The options can be set in the state new only. The first creation that
 * passes the checks of the document fixes them.
 * - The document is checked and copied when the file is created, so later
 *   changes of it have no effect.
 * - A call that the state does not allow returns X3PIO_ERR_STATE, and an
 * invalid argument returns X3PIO_ERR_ARGUMENT. Neither changes the state or the
 *   position in the file.
 * - A creation that fails leaves the writer new, so it can create the file
 *   again.
 * - A writer that is released before x3pio_end_file() leaves an incomplete
 * file.
 *
 * @par Missing Points
 * A point that was not measured is NaN or infinity in a buffer of metres, and
 * in a file of floats NaN in z. A file of integers cannot hold it in the
 * values, and keeps a file of validity bits: the `valid` field of
 * x3pio_write_data() with 0 for a point that was not measured. A point cloud
 * has none.
 */

#ifndef X3PIO_WRITER_H
#define X3PIO_WRITER_H

#include <x3pio/document.h>
#include <x3pio/layout.h>
#include <x3pio/types.h>
#include <x3pio/x3pio_export.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Opaque handle of a writer. */
typedef struct x3pio_writer x3pio_writer;

/**
 * Creates a writer. The writer writes nothing until a file is created.
 *
 * Only the handle is allocated here. The program may set the options of the
 * writer, and the buffers are allocated when a file is created. Release the
 * writer with x3pio_writer_free().
 *
 * @param[out] writer Receives the writer, or null if the call fails.
 * @retval X3PIO_ERR_ARGUMENT `writer` is null.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
X3PIO_API x3pio_status x3pio_writer_create(x3pio_writer **writer);

/**
 * Sets how the points are stored: in a binary file of the container, which is
 * compact and fast and the default, or as text in main.xml, which is readable.
 * The text format is meant for small files, of no more than about 10,000
 * points. The numbers of all points are collected in memory until the file
 * ends.
 *
 * @param[in,out] writer The writer.
 * @param storage The storage.
 * @retval X3PIO_ERR_ARGUMENT The writer is null or the value is not an
 * enumerator.
 * @retval X3PIO_ERR_STATE An attempt to create a file was made earlier.
 */
X3PIO_API x3pio_status x3pio_writer_set_storage(x3pio_writer *writer,
                                                x3pio_storage storage);

/**
 * Sets how the members of the container are compressed: the points, main.xml
 * and the files of vendors that have no setting of their own. The default is
 * deflate at the default level. A lower level is faster and a higher level
 * gives a smaller file. The method store writes the numbers as they are, in a
 * zip file that every program reads.
 *
 * @param[in,out] writer The writer.
 * @param[in] compression The compression.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, or the method or the level is
 * not valid.
 * @retval X3PIO_ERR_STATE An attempt to create a file was made earlier.
 */
X3PIO_API x3pio_status x3pio_writer_set_compression(
    x3pio_writer *writer, const x3pio_compression *compression);

/**
 * Sets the number of points that the writer converts at a time. The default is
 * 8192.
 *
 * @param[in,out] writer The writer.
 * @param count The number of points, not 0 and not so large that the chunk
 * cannot be counted in bytes.
 * @retval X3PIO_ERR_ARGUMENT The writer is null or the number is not valid.
 * @retval X3PIO_ERR_STATE An attempt to create a file was made earlier.
 */
X3PIO_API x3pio_status x3pio_writer_set_chunk(x3pio_writer *writer,
                                              size_t count);

/**
 * Releases a writer. A file that x3pio_create_file() created and
 * x3pio_end_file() did not end is closed in its incomplete state. A null
 * pointer is ignored.
 *
 * @param[in,out] writer The writer.
 */
X3PIO_API void x3pio_writer_free(x3pio_writer *writer);

/**
 * Starts a file in memory. The writer collects the container in memory, because
 * a zip file is not written in one go. After x3pio_end_file(),
 * x3pio_writer_memory() returns the bytes, which the program can write where it
 * wants, for example to a network connection.
 *
 * @param[in,out] writer The writer.
 * @param[in] document The description of the file.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, or the document cannot be
 * written. The document is checked and copied, its revision decides the form of
 * the file, and its points, links and vendor identifiers are not used.
 * @retval X3PIO_ERR_STATE The writer has created a file already.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
X3PIO_API x3pio_status x3pio_create_memory(x3pio_writer *writer,
                                           const x3pio_document *document);

/**
 * Returns the bytes of a file that x3pio_create_memory() started and
 * x3pio_end_file() completed. The bytes are the writer's: they remain valid
 * until the writer is freed.
 *
 * @param[in] writer The writer.
 * @param[out] data Receives the address of the bytes.
 * @param[out] size Receives the number of bytes.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null.
 * @retval X3PIO_ERR_STATE The file is not in memory, or is not ended.
 */
X3PIO_API x3pio_status x3pio_writer_memory(const x3pio_writer *writer,
                                           const void **data, size_t *size);

/**
 * Writes the next rows of points as the file stores them.
 *
 * Each field that is bound holds `rows` times `x3pio_document_columns()`
 * values, in the order of the file and in the byte order of the machine. The
 * buffer lies in memory as its layout describes, and the type of the layout has
 * to be the type in which the file stores the axis, as for x3pio_read_data().
 * The numbers are written as they are.
 *
 * - Every axis that the file stores has to be bound, and an axis that it does
 * not store, an incremental one, cannot be.
 * - `valid` marks the points that were not measured by 0. It is how integers
 *   carry them; without it all points count as measured. A point of a file of
 *   floats is also not measured when a coordinate is not a number. A point
 * cloud cannot hold a point that was not measured.
 * - The z axis of the document must have its increment set, not
 *   X3PIO_AUTO_INCREMENT.
 *
 * The first rows written are the first rows stored in the file.
 *
 * @param[in,out] writer The writer.
 * @param[in] from The buffers to write.
 * @param rows The number of rows to write. A list of points has a row for each
 * point.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, no field is bound, `rows`
 * exceeds the rows remaining, the type or the strides of a layout are invalid,
 * an axis that the file stores is not bound or one that it does not is, a point
 * of a cloud was not measured, or the increment is not set.
 * @retval X3PIO_ERR_STATE No file is created, or an earlier error ended the
 * writing.
 * @retval X3PIO_ERR_IO The file or the container could not be written.
 */
X3PIO_API x3pio_status x3pio_write_data(x3pio_writer *writer,
                                        const x3pio_const_fields *from,
                                        size_t rows);

/**
 * Writes the next rows of points from coordinates in metres in the view
 * coordinate system of the file, from buffers of floats or doubles. A stored
 * value is a coordinate divided by the increment of its axis, rounded to the
 * nearest value for an integer type, a tie to the even one. The rules of
 * x3pio_write_data() apply, except for the types and the mask: the type of a
 * layout is X3PIO_TYPE_FLOAT or X3PIO_TYPE_DOUBLE, NaN and infinity mark the
 * points that were not measured, and `valid` cannot be bound.
 *
 * A program that has coordinates in the global coordinate system applies
 * x3pio_placement_to_view() to a point first.
 *
 * @param[in,out] writer The writer.
 * @param[in] from The buffers to write.
 * @param rows The number of rows to write. A list of points has a row for each
 * point.
 * @retval X3PIO_ERR_ARGUMENT As for x3pio_write_data(), and `valid` is bound.
 * @retval X3PIO_ERR_RANGE A coordinate does not fit the type of its axis. The
 * error ends the writing.
 * @return Otherwise the errors of x3pio_write_data().
 */
X3PIO_API x3pio_status x3pio_write_points(x3pio_writer *writer,
                                          const x3pio_const_fields *from,
                                          size_t rows);

/**
 * Writes all rows from a matrix of heights in metres that lies in memory as the
 * layout describes. x3pio_layout_packed() describes a plain array. The file
 * must store heights alone, that is, its x and y axes are incremental.
 *
 * The layout is that of x3pio_read_heights(): the element (y, x) is at
 * `src + y * layout.row_stride + x * layout.element_stride`, and the row y = 0
 * is the first row. For an integer type with X3PIO_AUTO_INCREMENT the increment
 * is selected from the heights. NaN and infinity label points that were not
 * measured. Call x3pio_end_file() afterwards.
 *
 * @param[in,out] writer The writer.
 * @param[in] layout The layout of the buffer, of floats or doubles.
 * @param[in] src The heights.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, the layout is invalid, or the
 * file stores x and y.
 * @retval X3PIO_ERR_STATE No file is created, an earlier error ended the
 * writing, or rows were written already.
 * @retval X3PIO_ERR_RANGE A height does not fit the type of the axis with its
 * increment.
 * @retval X3PIO_ERR_IO The file or the container could not be written.
 */
X3PIO_API x3pio_status x3pio_write_heights(x3pio_writer *writer,
                                           const x3pio_layout *layout,
                                           const void *src);

/**
 * Adds a file of a vendor to the container.
 *
 * In the revision of Amd 1 every file has an identifier of its own, a URI with
 * a host name and a path to a file, which names the file in the container
 * unless `member` is given. In the revision of 2017 all files share one
 * identifier, and `member`, the name in the container, is required. The content
 * is copied, and the file is written when the file ends.
 *
 * @param[in,out] writer The writer.
 * @param[in] id The vendor identifier.
 * @param[in] member The name in the container, or null.
 * @param[in] content The bytes of the file. May be null if the size is 0.
 * @param size The size of the file in bytes.
 * @param[in] compression The compression of this file, or null for the one of
 * the writer. A file that is compressed already, such as a PNG image, is best
 * stored.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, or the identifier or the name
 * cannot be used: it is taken or reserved, or leaves the container.
 * @retval X3PIO_ERR_STATE No file is created, or it is ended.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
X3PIO_API x3pio_status x3pio_add_vendor_file(
    x3pio_writer *writer, const char *id, const char *member,
    const void *content, size_t size, const x3pio_compression *compression);

/**
 * Ends the file: writes main.xml and the checksum, completes the container, and
 * for a file that x3pio_create_file() created closes the file. All rows of the
 * document must be written. Nothing can be written afterwards.
 *
 * @param[in,out] writer The writer.
 * @retval X3PIO_ERR_ARGUMENT The writer is null.
 * @retval X3PIO_ERR_STATE No file is created, rows are missing, the file is
 * ended already, or an earlier error ended the writing.
 * @retval X3PIO_ERR_IO The file could not be written.
 */
X3PIO_API x3pio_status x3pio_end_file(x3pio_writer *writer);

/**
 * Returns a description of the most recent failure of the writer, or an empty
 * string if none occurred. The result is never null, and a string of the
 * library, valid for the life of the program.
 *
 * @param[in] writer The writer.
 */
X3PIO_API const char *x3pio_writer_error_message(const x3pio_writer *writer);

#ifdef __cplusplus
}
#endif

#endif /* X3PIO_WRITER_H */
