/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Layout of a buffer in memory, and the fields of a point that are bound to
 * buffers.
 *
 * A layout lets the functions transfer a matrix in place when it is part of
 * something else: a member of a structure, a column-major matrix or a matrix
 * upside down. The program allocates every buffer.
 */

#ifndef X3PIO_LAYOUT_H
#define X3PIO_LAYOUT_H

#include <x3pio/types.h>
#include <x3pio/x3pio_export.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Description of a buffer of the program for the values of one field of the
 * points: the type of its elements, and how the matrix lies in memory.
 *
 * The matrix has the columns and the rows of the file. A file of a matrix has
 * `size_x` columns and `size_y` times `size_z` rows, which are the rows of the
 * first layer, then the rows of the second layer, and so on; a file of a list
 * of points has all points in one row. The element (y, x) of a buffer is at
 * `data + y * row_stride + x * element_stride`. Both strides can be negative.
 * They must keep the elements apart: the strides of a dimension of more than
 * one element must not be 0 after the defaults below are applied. This allows
 * values to be read or written in place where they are not contiguous, for
 * example the z member of an array of points, one channel of an interleaved
 * image, a column-major matrix or a matrix upside down. x3pio_layout_packed()
 * describes a plain array. A program that does not use an initializer calls
 * x3pio_layout_init() before it assigns fields.
 *
 * The structure is frozen for the major version: it has these three fields and
 * a new property of a buffer is a new function.
 */
typedef struct x3pio_layout
{
    /**
     * Type of the elements of the buffer. A buffer of metres has the type
     * X3PIO_TYPE_FLOAT or X3PIO_TYPE_DOUBLE. A buffer of stored values has the
     * type in which the file stores the axis, and a buffer of validity has the
     * type X3PIO_TYPE_UINT8.
     */
    x3pio_type type;
    /**
     * Distance in bytes between consecutive elements within a row. 0 selects
     * elements that follow each other directly.
     */
    ptrdiff_t element_stride;
    /**
     * Distance in bytes between consecutive rows. 0 selects rows that follow
     * each other, each holding the columns of the matrix at `element_stride`.
     * A column-major matrix needs an explicit value.
     */
    ptrdiff_t row_stride;
} x3pio_layout;

/**
 * Initializes a layout of a plain array of `double` elements: the elements
 * follow each other and the rows follow each other. The caller sets the type
 * and every other field that differs.
 *
 * @param[out] layout The layout to initialize.
 */
X3PIO_API void x3pio_layout_init(x3pio_layout *layout);

/**
 * Returns the layout of a plain array: the elements follow each other and the
 * rows follow each other. The layout is a constant of the library, valid for
 * the life of the program, and the same for every thread. A program that needs
 * other strides copies it, or uses x3pio_layout_init().
 *
 * @param type The type of the elements.
 * @return The layout, or null if `type` is not a type.
 */
X3PIO_API const x3pio_layout *x3pio_layout_packed(x3pio_type type);

/**
 * A buffer of the program that the library writes into: where it is, and how it
 * lies in memory. A field without an address is not bound.
 */
typedef struct x3pio_field
{
    /** The element in the first row and the first column. */
    void *data;
    /** How the matrix lies in memory. */
    x3pio_layout layout;
} x3pio_field;

/**
 * A buffer of the program that the library reads from. It is the same as
 * x3pio_field with the address of constant data.
 */
typedef struct x3pio_const_field
{
    /** The element in the first row and the first column. */
    const void *data;
    /** How the matrix lies in memory. */
    x3pio_layout layout;
} x3pio_const_field;

/**
 * The buffers that a read fills, one for each field of a point. A field without
 * an address is not read.
 *
 * To read a depth image, bind only `z`. To fill an array of structures, bind x,
 * y and z to their members, with the size of the structure as the element
 * stride. `valid` is the mask of the points, one byte for each of them of the
 * type X3PIO_TYPE_UINT8: 1 for a point that was measured, and 0 for a point
 * that was not. It is how integers, which have no value for a point that was
 * not measured, carry them. A program that does not use a designated
 * initializer calls x3pio_fields_init() first.
 */
typedef struct x3pio_fields
{
    /** The x coordinates. */
    x3pio_field x;
    /** The y coordinates. */
    x3pio_field y;
    /** The z coordinates, the heights. */
    x3pio_field z;
    /** The mask of the points: 1 for measured, 0 for missing. */
    x3pio_field valid;
} x3pio_fields;

/**
 * The buffers that a write reads from, the counterpart of x3pio_fields.
 */
typedef struct x3pio_const_fields
{
    /** The x coordinates. */
    x3pio_const_field x;
    /** The y coordinates. */
    x3pio_const_field y;
    /** The z coordinates, the heights. */
    x3pio_const_field z;
    /** The mask of the points: nonzero for measured, 0 for missing. */
    x3pio_const_field valid;
} x3pio_const_fields;

/**
 * Initializes fields: none is bound.
 *
 * @param[out] fields The fields.
 */
X3PIO_API void x3pio_fields_init(x3pio_fields *fields);

/**
 * Initializes fields for a write: none is bound.
 *
 * @param[out] fields The fields.
 */
X3PIO_API void x3pio_const_fields_init(x3pio_const_fields *fields);

/**
 * Packs a mask of one byte for each point into the bits that a file holds: the
 * point j is bit j % 8 of byte j / 8, with the least significant bit first, and
 * 1 for a measured point. The bits behind the last point are 0.
 *
 * @param[in] flags The mask, a byte for each point, 0 for a point that was not
 * measured and any other value for one that was.
 * @param count The number of points.
 * @param[out] bits Receives (count + 7) / 8 bytes.
 */
X3PIO_API void x3pio_validity_pack(const unsigned char *flags, size_t count,
                                   unsigned char *bits);

/**
 * Unpacks the bits of a mask into one byte for each point, 1 or 0. It is the
 * reverse of x3pio_validity_pack().
 *
 * @param[in] bits The bits, (count + 7) / 8 bytes.
 * @param count The number of points.
 * @param[out] flags Receives the mask.
 */
X3PIO_API void x3pio_validity_unpack(const unsigned char *bits, size_t count,
                                     unsigned char *flags);

#ifdef __cplusplus
}
#endif

#endif /* X3PIO_LAYOUT_H */
