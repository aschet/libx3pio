/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Reader of a file or of memory.
 *
 * @par Process
 * A reader reads one file at a time, from the start to the end:
 * -# x3pio_reader_create() creates the reader, and x3pio_reader_set_verify()
 *    and x3pio_reader_set_chunk() set the options.
 * -# x3pio_open_file() or x3pio_open_memory() opens the
 *    file and reads its document, which x3pio_reader_document() returns.
 * -# x3pio_read_z() reads the z coordinates of all points, or
 *    x3pio_read_points() or x3pio_read_data() reads 3-d points in metres or
 *    as the file stores them, in parts, in the order of the file.
 * -# x3pio_reader_free() releases the reader.
 *
 * @par States
 * A reader is new until a file is opened, then in the state data. An error that
 * loses the position in the file moves it to failed: X3PIO_ERR_IO,
 * X3PIO_ERR_FORMAT, X3PIO_ERR_CHECKSUM or X3PIO_ERR_RANGE in a call that reads.
 * The document stays available in every state after the opening.
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
 * the file is opened and the point data when its last point has been read, and
 * fails with X3PIO_ERR_CHECKSUM on a mismatch.
 *
 * @par Selective Reading
 * A zip file lists its members at its end, so a reader needs the whole file to
 * be available, on disk or in memory, and reads the member that a call needs
 * and nothing else: the points are read from their member in parts, and a file
 * of a vendor is read only by x3pio_read_member(). A program that receives a
 * file as a stream, for example from a pipe, collects the bytes in a buffer
 * first and opens that with x3pio_open_memory().
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
 * @param[out] reader Receives the reader, or null if the call fails.
 * @retval X3PIO_ERR_ARGUMENT `reader` is null.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
X3PIO_API x3pio_status x3pio_reader_create(x3pio_reader **reader);

/**
 * Sets whether the reader checks the checksums that the file states. The
 * default is true.
 *
 * @param[in,out] reader The reader.
 * @param verify Whether to check.
 * @retval X3PIO_ERR_ARGUMENT The reader is null.
 * @retval X3PIO_ERR_STATE The reader tried to open a file earlier.
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
 * @retval X3PIO_ERR_STATE The reader tried to open a file earlier.
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
 * Opens a block of memory as the source and reads the document. The memory
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
 * Returns the document of the opened file, or null if no file is open. The
 * pointer remains valid until the reader is freed.
 *
 * @param[in] reader The reader.
 */
X3PIO_API const x3pio_document *
x3pio_reader_document(const x3pio_reader *reader);

/**
 * Reads the next 3-d points as the file stores them.
 *
 * Each field that is bound receives `points` values, in the order of the file,
 * in the byte order of the machine and without conversion. The order is that
 * of the standard: u is the fastest index, v the second and w, the layer, the
 * slowest. A list of points takes any number of points. A matrix is read in
 * whole runs along u, so `points` is a multiple of `size_x`.
 *
 * The buffer lies in memory as its layout specifies, and the type of the layout
 * has to be the type in which the file stores the axis. With n the size along
 * u of a matrix, and 1 for a list, the k-th point of the call is stored at
 * `data + (k / n) * layout.row_stride + (k % n) * layout.element_stride`.
 * x3pio_layout_packed() specifies values that follow each other.
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
 * The first points read are the first points stored in the file. After an error
 * the reader rejects further reads.
 *
 * @param[in,out] reader The reader.
 * @param[in] into The buffers to fill.
 * @param points The number of 3-d points to read.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, no field is bound, `points`
 * exceeds the points remaining or is not a multiple of `size_x` for a matrix,
 * the type or the strides of a layout are invalid, or an axis that the file
 * does not store is bound.
 * @retval X3PIO_ERR_STATE No file is open, or an earlier error ended the
 * reading.
 * @retval X3PIO_ERR_IO The file could not be read.
 * @retval X3PIO_ERR_FORMAT The point data is damaged or truncated.
 * @retval X3PIO_ERR_CHECKSUM The checksum of the point data, checked after the
 * last point, does not match.
 */
X3PIO_API x3pio_status x3pio_read_data(x3pio_reader *reader,
                                       const x3pio_fields *into, size_t points);

/**
 * Reads the next 3-d points as coordinates in metres in the view coordinate
 * system of the file, into buffers of floats or doubles. A coordinate is a
 * stored value times the increment of its axis. An incremental axis gives the
 * index of the point times its increment, so x and y of a regular grid can be
 * bound too. The rules of x3pio_read_data() apply, except for the types and the
 * mask: the type of a layout is X3PIO_TYPE_FLOAT or X3PIO_TYPE_DOUBLE, and a
 * point that was not measured is NaN in every bound field that the file stores,
 * whereas `valid` cannot be bound.
 *
 * The coordinates are not rotated or shifted. x3pio_read_points_in() reads them
 * in the other coordinate systems.
 *
 * @param[in,out] reader The reader.
 * @param[in] into The buffers to fill.
 * @param points The number of 3-d points to read.
 * @retval X3PIO_ERR_ARGUMENT As for x3pio_read_data(), and `valid` is bound.
 * @retval X3PIO_ERR_RANGE A coordinate does not fit the type of its buffer.
 * @return Otherwise the errors of x3pio_read_data().
 */
X3PIO_API x3pio_status x3pio_read_points(x3pio_reader *reader,
                                         const x3pio_fields *into,
                                         size_t points);

/**
 * Reads the next 3-d points as coordinates in metres in a coordinate system
 * that the program chooses, see x3pio_coordinate_system. With
 * X3PIO_COORDINATES_VIEW it is x3pio_read_points().
 * X3PIO_COORDINATES_VIEW_OFFSET adds the offset of each axis, and
 * X3PIO_COORDINATES_GLOBAL applies the rotation and the offset of the
 * transformation. The transformation is applied in double precision before a
 * coordinate is stored in the type of its buffer, which matters for buffers of
 * floats and for transformations with a large offset: a program that applies
 * the transformation to floats itself loses digits.
 *
 * The rotation acts on a whole point, so with X3PIO_COORDINATES_GLOBAL x, y and
 * z have to be bound, and a point that was not measured is NaN in all three
 * coordinates. A shift acts on each axis alone, so
 * X3PIO_COORDINATES_VIEW_OFFSET binds any of the fields, for example z alone.
 * The rules of x3pio_read_points() apply otherwise, and calls with different
 * coordinate systems may follow each other.
 *
 * @param[in,out] reader The reader.
 * @param system The coordinate system of the buffers.
 * @param[in] into The buffers to fill.
 * @param points The number of 3-d points to read.
 * @retval X3PIO_ERR_ARGUMENT As for x3pio_read_points(), `system` is not a
 * coordinate system, or x, y or z is not bound with X3PIO_COORDINATES_GLOBAL.
 * @return Otherwise the errors of x3pio_read_points().
 */
X3PIO_API x3pio_status x3pio_read_points_in(x3pio_reader *reader,
                                            x3pio_coordinate_system system,
                                            const x3pio_fields *into,
                                            size_t points);

/**
 * Reads the z coordinates of all points in metres into a buffer that lies in
 * memory as the layout specifies. x3pio_layout_packed() specifies a plain
 * array. It is x3pio_read_points() for z alone, for all points: the layers
 * follow each other.
 *
 * The point (u, v) of the first layer is stored at `dst + v * layout.row_stride
 * + u * layout.element_stride`, and the next layer follows the runs of the
 * first one. A negative row stride, with `dst` at the first element of v = 0,
 * which is the last run in memory, stores the matrix upside down. A point that
 * was not measured becomes NaN.
 *
 * The function reads from the first point of the data, so no points may have
 * been read before.
 *
 * @param[in,out] reader The reader.
 * @param[in] layout The layout of the buffer, of floats or doubles.
 * @param[out] dst The buffer for the z coordinates.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null or the layout is invalid.
 * @retval X3PIO_ERR_STATE No file is open, an earlier error ended the reading,
 * or points were read already.
 * @return Otherwise the errors of x3pio_read_points().
 */
X3PIO_API x3pio_status x3pio_read_z(x3pio_reader *reader,
                                    const x3pio_layout *layout, void *dst);

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
 * Returns the text of the most recent failure of the reader, or an empty
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
