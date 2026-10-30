/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Reader of a file, memory or a stream.
 *
 * @par Process
 * A reader reads one file at a time, from the start to the end:
 * -# x3pio_reader_create() creates the reader, and x3pio_reader_set_verify()
 *    and x3pio_reader_set_chunk() set the options.
 * -# x3pio_open_file(), x3pio_open_memory() or x3pio_open_stream() opens the
 *    file and reads its description, which x3pio_reader_document() returns.
 * -# x3pio_read_heights() reads the whole matrix of heights, or
 *    x3pio_read_points() or x3pio_read_data() reads the rows of points in
 *    metres or as the file stores them, in parts of one row or more, in the
 *    order of the file.
 * -# x3pio_reader_free() releases the reader.
 *
 * @par States
 * A reader is new until a file is opened, then in the state data. An error that
 * loses the position in the file moves it to failed: X3PIO_ERR_IO,
 * X3PIO_ERR_FORMAT, X3PIO_ERR_CHECKSUM or X3PIO_ERR_RANGE in a call that reads.
 * The description stays available in every state after the opening.
 * - The options can be set in the state new only. The first attempt to open a
 *   file fixes them.
 * - A call that the state does not allow returns X3PIO_ERR_STATE, and an
 * invalid argument returns X3PIO_ERR_ARGUMENT. Neither changes the state or the
 *   position in the file.
 * - An opening that fails leaves the reader new, so it can open another file.
 *
 * @par Checksums
 * A file states the MD5 checksums of main.xml, of the point data and of the
 * validity bits. By default a reader checks main.xml and the validity bits when
 * the file is opened and the point data when its last row has been read, and
 * fails with X3PIO_ERR_CHECKSUM on a mismatch.
 *
 * @par Selective Reading
 * A zip file lists its members at its end, so a reader of a file or of memory
 * reads the member that a call needs and nothing else: the points are read from
 * their member in parts, and a file of a vendor is read only by
 * x3pio_read_member(). A stream cannot be read from its end, so
 * x3pio_open_stream() collects it in memory first.
 */

#ifndef X3PIO_READER_H
#define X3PIO_READER_H

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

/** Opaque handle of a reader. */
typedef struct x3pio_reader x3pio_reader;

/**
 * Creates a reader. The reader reads nothing until a file is opened.
 *
 * Only the handle is allocated here. The program may set the options of the
 * reader, and the buffers are allocated when a file is opened. Release the
 * reader with x3pio_reader_free().
 *
 * @param[in] allocator The allocator for the memory of the reader, for example
 * x3pio_default_allocator(). The structure is copied, so it need not outlive
 * the call. Both callbacks must be set. Null selects the default.
 * @param[out] reader Receives the reader, or null if the call fails.
 * @retval X3PIO_ERR_ARGUMENT `reader` is null, or the allocator lacks a
 * callback.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
X3PIO_API x3pio_status x3pio_reader_create(const x3pio_allocator *allocator,
                                           x3pio_reader **reader);

/**
 * Sets whether the reader checks the checksums that the file states. The
 * default is true.
 *
 * @param[in,out] reader The reader.
 * @param verify Whether to check.
 * @retval X3PIO_ERR_ARGUMENT The reader is null.
 * @retval X3PIO_ERR_STATE An attempt to open a file was made earlier.
 */
X3PIO_API x3pio_status x3pio_reader_set_verify(x3pio_reader *reader,
                                               bool verify);

/**
 * Sets the number of points that the reader converts at a time. The default is
 * 8192. The chunk occupies up to 7 doubles and a byte per point, and a smaller
 * chunk costs more calls of the compression library.
 *
 * @param[in,out] reader The reader.
 * @param count The number of points, not 0 and not so large that the chunk
 * cannot be counted in bytes.
 * @retval X3PIO_ERR_ARGUMENT The reader is null or the number is not valid.
 * @retval X3PIO_ERR_STATE An attempt to open a file was made earlier.
 */
X3PIO_API x3pio_status x3pio_reader_set_chunk(x3pio_reader *reader,
                                              size_t count);

/**
 * Releases a reader and closes the file that it has open. A null pointer is
 * ignored.
 *
 * @param[in,out] reader The reader.
 */
X3PIO_API void x3pio_reader_free(x3pio_reader *reader);

/**
 * Opens a block of memory as the source and reads the description. The memory
 * is not copied: it must remain valid and unchanged while the reader is in use.
 *
 * @param[in,out] reader The reader.
 * @param[in] data The first byte of the file.
 * @param size The size of the file in bytes.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null.
 * @retval X3PIO_ERR_STATE The reader is open already.
 * @retval X3PIO_ERR_FORMAT The data is not an x3p file or is damaged.
 * @retval X3PIO_ERR_CHECKSUM The checksum of main.xml or of the validity bits
 * does not match.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
X3PIO_API x3pio_status x3pio_open_memory(x3pio_reader *reader, const void *data,
                                         size_t size);

/**
 * Opens a stream as the source and reads the description. The reader collects
 * the whole stream in memory first, because a zip file is read from its end,
 * and then does not use the stream any more. The structure is copied, and the
 * object that `user` refers to must remain valid during the call only.
 *
 * @param[in,out] reader The reader.
 * @param[in] stream The stream; `read` is required.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null or `read` is not set.
 * @retval X3PIO_ERR_STATE The reader is open already.
 * @retval X3PIO_ERR_IO The stream failed.
 * @return Otherwise the result of x3pio_open_memory().
 */
X3PIO_API x3pio_status x3pio_open_stream(x3pio_reader *reader,
                                         const x3pio_stream *stream);

/**
 * Returns the description of the opened file, or null if no file is open. The
 * pointer remains valid until the reader is freed.
 *
 * @param[in] reader The reader.
 */
X3PIO_API const x3pio_document *
x3pio_reader_document(const x3pio_reader *reader);

/**
 * Reads the next rows of points as the file stores them.
 *
 * Each field that is bound receives `rows` times `x3pio_document_columns()`
 * values, in the order of the file, in the byte order of the machine and
 * without conversion. The buffer lies in memory as its layout describes, and
 * the type of the layout has to be the type in which the file stores the axis.
 * The element (y, x) is stored at `data + y * layout.row_stride + x *
 * layout.element_stride`, where y counts the rows from the first row that is
 * read. x3pio_layout_packed() describes values that follow each other.
 *
 * - An axis that the file does not store, an incremental one, cannot be bound.
 * - `valid` receives the mask of the points: 1 for a point that was measured
 * and 0 for one that was not, in bytes of the type X3PIO_TYPE_UINT8. A file of
 *   integers has no value for a point that was not measured, and a mask in its
 *   place; x3pio_document_has_validity() tells whether the file has one.
 * Without a mask all points count as measured. A point of a file of floats is
 * also not measured when a coordinate is not a number.
 * - The values of a point that was not measured are those of the file.
 *
 * The first rows read are the first rows stored in the file. After an error the
 * reader rejects further reads.
 *
 * @param[in,out] reader The reader.
 * @param[in] into The buffers to fill.
 * @param rows The number of rows to read. A list of points is one row.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, no field is bound, `rows`
 * exceeds the rows remaining, the type or the strides of a layout are invalid,
 * or an axis that the file does not store is bound.
 * @retval X3PIO_ERR_STATE No file is open, or an earlier error ended the
 * reading.
 * @retval X3PIO_ERR_IO The file could not be read.
 * @retval X3PIO_ERR_FORMAT The point data is damaged or truncated.
 * @retval X3PIO_ERR_CHECKSUM The checksum of the point data, checked after the
 * last row, does not match.
 */
X3PIO_API x3pio_status x3pio_read_data(x3pio_reader *reader,
                                       const x3pio_fields *into, size_t rows);

/**
 * Reads the next rows of points as coordinates in metres in the view coordinate
 * system of the file, into buffers of floats or doubles. A coordinate is a
 * stored value times the increment of its axis. An incremental axis gives the
 * index of the point times its increment, so x and y of a regular grid can be
 * bound too. The rules of x3pio_read_data() apply, except for the types and the
 * mask: the type of a layout is X3PIO_TYPE_FLOAT or X3PIO_TYPE_DOUBLE, and a
 * point that was not measured is NaN in every bound field that the file stores,
 * whereas `valid` cannot be bound.
 *
 * The coordinates are not rotated or shifted. A program that wants the global
 * coordinate system applies x3pio_placement_to_global() to a point.
 *
 * @param[in,out] reader The reader.
 * @param[in] into The buffers to fill.
 * @param rows The number of rows to read. A list of points is one row.
 * @retval X3PIO_ERR_ARGUMENT As for x3pio_read_data(), and `valid` is bound.
 * @retval X3PIO_ERR_RANGE A coordinate does not fit the type of its buffer.
 * @return Otherwise the errors of x3pio_read_data().
 */
X3PIO_API x3pio_status x3pio_read_points(x3pio_reader *reader,
                                         const x3pio_fields *into, size_t rows);

/**
 * Reads the remaining rows as heights in metres into a buffer that lies in
 * memory as the layout describes. x3pio_layout_packed() describes a plain
 * array. It is x3pio_read_points() for z alone, for all rows: the rows of the
 * layers follow each other.
 *
 * The element (y, x) is stored at `dst + y * layout.row_stride + x *
 * layout.element_stride`. The row y = 0 is the first row. A negative row
 * stride, with `dst` at the first element of the row y = 0, which is the last
 * row in memory, stores the matrix upside down. A point that was not measured
 * becomes NaN.
 *
 * The function reads from the first row of the data, so no rows may have been
 * read before.
 *
 * @param[in,out] reader The reader.
 * @param[in] layout The layout of the buffer, of floats or doubles.
 * @param[out] dst The buffer for the heights.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null or the layout is invalid.
 * @retval X3PIO_ERR_STATE No file is open, an earlier error ended the reading,
 * or rows were read already.
 * @return Otherwise the errors of x3pio_read_points().
 */
X3PIO_API x3pio_status x3pio_read_heights(x3pio_reader *reader,
                                          const x3pio_layout *layout,
                                          void *dst);

/**
 * Returns the number of files of vendors that the container holds.
 *
 * In the revision of Amd 1 each vendor identifier names a file. In the revision
 * of 2017 the one vendor owns every file of the container that is not part of
 * the format. A vendor identifier that names no file in the container counts as
 * a file without a member.
 *
 * @param[in] reader The reader. May be null, which has none.
 */
X3PIO_API size_t x3pio_reader_vendor_file_count(const x3pio_reader *reader);

/**
 * Returns the vendor identifier of a file of a vendor.
 *
 * @param[in] reader The reader.
 * @param index A number below x3pio_reader_vendor_file_count().
 * @return The identifier, or null if the index is not below the count.
 */
X3PIO_API const char *x3pio_reader_vendor_file_id(const x3pio_reader *reader,
                                                  size_t index);

/**
 * Returns the name in the container of a file of a vendor.
 *
 * @param[in] reader The reader.
 * @param index A number below x3pio_reader_vendor_file_count().
 * @return The name, or null if the index is not below the count or the
 * identifier has no file.
 */
X3PIO_API const char *
x3pio_reader_vendor_file_member(const x3pio_reader *reader, size_t index);

/**
 * Reads a member of the container, for example a file of a vendor or the
 * original main.xml. Only this member is read.
 *
 * The call copies the member to the buffer if it fits. A program that does not
 * know the size calls it with a capacity of 0 and then with a buffer of the
 * size it returned.
 *
 * @param[in,out] reader The reader.
 * @param[in] member The name in the container, in which a backslash separates
 * like a slash.
 * @param[out] buffer Receives the bytes. May be null if the capacity is 0.
 * @param capacity The size of `buffer` in bytes.
 * @param[out] size Receives the size of the member in bytes.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, or the name does not point to a
 * member of the container.
 * @retval X3PIO_ERR_STATE No file is open.
 * @retval X3PIO_ERR_RANGE The member does not fit the buffer. `size` is set.
 * @retval X3PIO_ERR_IO The container has no such member, or it could not be
 * read.
 * @retval X3PIO_ERR_FORMAT The member is damaged.
 */
X3PIO_API x3pio_status x3pio_read_member(x3pio_reader *reader,
                                         const char *member, void *buffer,
                                         size_t capacity, size_t *size);

/**
 * Returns a description of the most recent failure of the reader, or an empty
 * string if none occurred. The result is never null, and a string of the
 * library, valid for the life of the program.
 *
 * @param[in] reader The reader.
 */
X3PIO_API const char *x3pio_reader_error_message(const x3pio_reader *reader);

#ifdef __cplusplus
}
#endif

#endif /* X3PIO_READER_H */
