/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Conversion between stored values and metres.
 *
 * @par Stored Values and Metres
 * A file holds the values of an absolute axis in a type: int16_t, int32_t,
 * float or double. A stored value multiplied by the increment of the axis is
 * the coordinate in metres in the view coordinate system. A program uses the
 * highest of three layers that suffices:
 * - x3pio_read_data() and x3pio_write_data() transfer the stored values as the
 *   file has them, in the type of the axis, with the mask of the points that
 *   were measured.
 * - x3pio_decode_values() and x3pio_encode_values() convert between stored
 *   values and metres. They need a document and no reader or writer.
 * - x3pio_read_points() and x3pio_write_points() transfer 3-d points in metres,
 *   and x3pio_read_z() and x3pio_write_z() transfer the z coordinates of all
 *   points.
 *
 * @par Points That Were Not Measured
 * A file of floats marks a point that was not measured by NaN in z. A file of
 * integers has no such value and keeps a mask of one bit for each point. Metres
 * mark it by NaN. The stored values of a point that was not measured are
 * whatever the file holds.
 *
 * @par Coordinate Systems
 * Metres are in the view coordinate system of the file. The transformation of
 * the document puts them in the global one: x3pio_read_points_in() and
 * x3pio_write_points_in() do it while the points are transferred, in double
 * precision, and x3pio_transformation_to_global() converts a single point.
 *
 * @par Increment
 * A file of integers trades range against resolution by the increment of its
 * axis. x3pio_suggest_increment_for_range() and x3pio_suggest_increment() give
 * the smallest increment that fits the values, and x3pio_write_z() uses
 * it if the z axis of the document has X3PIO_AUTO_INCREMENT.
 */

#ifndef X3PIO_CONVERT_H
#define X3PIO_CONVERT_H

#include <x3pio/document.h>
#include <x3pio/layout.h>
#include <x3pio/types.h>
#include <x3pio/x3pio_export.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Converts stored values of an axis into coordinates in metres.
 *
 * `count` values in the type of the axis are converted with its increment. The
 * coordinate i is stored at `metres + i * layout->element_stride`. A NaN stays
 * NaN. The function needs no handle, so it works on data from any source. The
 * axis has to be an absolute one.
 *
 * @param[in] document The document with the axes of the values.
 * @param coordinate The axis.
 * @param[in] stored The values, which follow each other.
 * @param count The number of values.
 * @param[in] layout The layout of the output buffer, of floats or doubles. Its
 * row stride is not used.
 * @param[out] metres The output buffer.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, the axis is not absolute, or
 * the layout is invalid.
 * @retval X3PIO_ERR_RANGE A coordinate does not fit the type of the buffer.
 */
X3PIO_API x3pio_status x3pio_decode_values(const x3pio_document *document,
                                           x3pio_coordinate coordinate,
                                           const void *stored, size_t count,
                                           const x3pio_layout *layout,
                                           void *metres);

/**
 * Converts coordinates in metres into stored values of an axis. It is the
 * inverse of x3pio_decode_values().
 *
 * The coordinates are divided by the increment of the axis. A NaN becomes 0 for
 * an integer type and stays NaN for a float type. Integer results are rounded
 * to the nearest value, a tie to the even one.
 *
 * @param[in] document The document with the axes of the values.
 * @param coordinate The axis.
 * @param[in] metres The coordinates.
 * @param count The number of coordinates.
 * @param[in] layout The layout of the input buffer, of floats or doubles. Its
 * row stride is not used.
 * @param[out] stored The values, which follow each other.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, the axis is not absolute, the
 * increment of the axis is not set, or the layout is invalid.
 * @retval X3PIO_ERR_RANGE A coordinate does not fit the type of the axis.
 */
X3PIO_API x3pio_status x3pio_encode_values(const x3pio_document *document,
                                           x3pio_coordinate coordinate,
                                           const void *metres, size_t count,
                                           const x3pio_layout *layout,
                                           void *stored);

/**
 * Returns the increment that uses the range of an integer type best for z
 * coordinates between `lowest` and `highest` metres, or 1 for a floating point
 * type.
 *
 * @param type The type of the axis.
 * @param lowest The lowest z coordinate in metres.
 * @param highest The highest z coordinate in metres.
 */
X3PIO_API double x3pio_suggest_increment_for_range(x3pio_type type,
                                                   double lowest,
                                                   double highest);

/**
 * Determines the increment for a matrix of z coordinates in metres, as
 * x3pio_write_z() does: the range of the finite z coordinates is determined and
 * passed to x3pio_suggest_increment_for_range(). It is for x3pio_write_data()
 * with an integer type.
 *
 * @param type The type of the axis.
 * @param[in] layout The layout of the buffer.
 * @param[in] z The z coordinates.
 * @param rows The number of rows of the matrix of the program.
 * @param columns The number of columns of the matrix of the program.
 * @param[out] increment Receives the increment.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null or the layout is invalid.
 */
X3PIO_API x3pio_status x3pio_suggest_increment(x3pio_type type,
                                               const x3pio_layout *layout,
                                               const void *z, size_t rows,
                                               size_t columns,
                                               double *increment);

#ifdef __cplusplus
}
#endif

#endif /* X3PIO_CONVERT_H */
