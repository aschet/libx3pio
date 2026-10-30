/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Encoding and decoding of the raw values of a file, and the transfer of
 * values from and to the buffers of the program.
 */

#ifndef X3PIO_SRC_CODEC_H
#define X3PIO_SRC_CODEC_H

#include "common.h"

/**
 * A buffer of the program as the internal functions see it: the layout with the
 * defaults applied, and the size of the matrix.
 */
typedef struct x3pio_matrix
{
    /**
     * The element in the first row and the first column. A matrix that is
     * written to has been made from a pointer to writable memory.
     */
    union
    {
        const unsigned char *read;
        unsigned char *write;
    } base;
    /** The type of the elements. */
    x3pio_type type;
    /** The distance in bytes between elements of a row. */
    ptrdiff_t element_stride;
    /** The distance in bytes between rows. */
    ptrdiff_t row_stride;
    /** The number of columns. */
    size_t columns;
} x3pio_matrix;

/**
 * Describes a buffer as a matrix.
 *
 * @param[out] matrix The matrix.
 * @param[in] data The element in the first row and the first column.
 * @param[in] layout The layout, whose zero strides take their defaults.
 * @param columns The number of columns.
 * @param rows The number of rows that the transfer touches.
 * @param writable Whether the library writes into the buffer; elements that lie
 * on top of each other cannot be.
 * @return False if the type of the layout does not exist, or the strides do not
 * keep the elements apart.
 */
bool x3pio_matrix_make(x3pio_matrix *matrix, const void *data,
                       const x3pio_layout *layout, size_t columns, size_t rows,
                       bool writable);

/**
 * Decodes values of a stored type from the little-endian bytes of a file into
 * doubles.
 *
 * @param type The type: int16_t, int32_t, float or double.
 * @param[in] src The bytes of the first value.
 * @param stride The distance in bytes between values.
 * @param[out] out Receives the values.
 * @param count The number of values.
 */
void x3pio_decode_samples(x3pio_type type, const unsigned char *src,
                          size_t stride, double *out, size_t count);

/**
 * Encodes doubles as little-endian values of a stored type. An integer is
 * rounded to the nearest, a tie to the even number, and a NaN becomes 0. A
 * float keeps a NaN.
 *
 * @param type The type: int16_t, int32_t, float or double.
 * @param[in] values The values.
 * @param count The number of values.
 * @param[out] dst Receives the bytes of the first value.
 * @param stride The distance in bytes between values.
 * @retval X3PIO_ERR_RANGE A finite value does not fit the type.
 */
x3pio_status x3pio_encode_samples(x3pio_type type, const double *values,
                                  size_t count, unsigned char *dst,
                                  size_t stride);

/**
 * Reads elements of a matrix, from the point `first` counted along the rows,
 * and converts them to doubles.
 *
 * @param[in] matrix The matrix.
 * @param first The index of the first element, row after row.
 * @param[out] out Receives the values.
 * @param count The number of values.
 */
void x3pio_matrix_load(const x3pio_matrix *matrix, size_t first, double *out,
                       size_t count);

/**
 * Converts doubles and writes them into a matrix, from the point `first`
 * counted along the rows. An integer is rounded to the nearest and a NaN
 * becomes 0.
 *
 * @param[in] matrix The matrix, which has been made writable.
 * @param first The index of the first element, row after row.
 * @param[in] values The values.
 * @param count The number of values.
 * @retval X3PIO_ERR_RANGE A finite value does not fit the type of the matrix.
 */
x3pio_status x3pio_matrix_store(const x3pio_matrix *matrix, size_t first,
                                const double *values, size_t count);

#endif /* X3PIO_SRC_CODEC_H */
