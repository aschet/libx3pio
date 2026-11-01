/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The buffers of the program as matrices, and the transfer of values
 * from and to them.
 */

#include "matrix.h"
#include "data_type.h"
#include <stddef.h>

/* Reads one element of a view, in the byte order of the machine. */
static double load_element(x3pio_type type, const unsigned char *p)
{
    switch (type)
    {
    case X3PIO_TYPE_INT8:
    {
        int8_t v;
        memcpy(&v, p, sizeof v);
        return (double)v;
    }
    case X3PIO_TYPE_UINT8:
        return (double)*p;
    case X3PIO_TYPE_INT16:
    {
        int16_t v;
        memcpy(&v, p, sizeof v);
        return (double)v;
    }
    case X3PIO_TYPE_UINT16:
    {
        uint16_t v;
        memcpy(&v, p, sizeof v);
        return (double)v;
    }
    case X3PIO_TYPE_INT32:
    {
        int32_t v;
        memcpy(&v, p, sizeof v);
        return (double)v;
    }
    case X3PIO_TYPE_UINT32:
    {
        uint32_t v;
        memcpy(&v, p, sizeof v);
        return (double)v;
    }
    case X3PIO_TYPE_FLOAT:
    {
        float v;
        memcpy(&v, p, sizeof v);
        return (double)v;
    }
    case X3PIO_TYPE_DOUBLE:
    {
        double v;
        memcpy(&v, p, sizeof v);
        return v;
    }
    }
    return 0.0;
}

/* Writes one element of a view, in the byte order of the machine. */
static bool store_element(x3pio_type type, unsigned char *p, double value)
{
    switch (type)
    {
    case X3PIO_TYPE_FLOAT:
    {
        if (!x3pio_fits_float(value))
        {
            return false;
        }
        const float v = (float)value;
        memcpy(p, &v, sizeof v);
        return true;
    }
    case X3PIO_TYPE_DOUBLE:
        memcpy(p, &value, sizeof value);
        return true;
    default:
        break;
    }
    double rounded = 0;
    if (!x3pio_round_to_integer(type, value, &rounded))
    {
        return false;
    }
    switch (type)
    {
    case X3PIO_TYPE_INT8:
    {
        const int8_t v = (int8_t)rounded;
        memcpy(p, &v, sizeof v);
        break;
    }
    case X3PIO_TYPE_UINT8:
        *p = (unsigned char)rounded;
        break;
    case X3PIO_TYPE_INT16:
    {
        const int16_t v = (int16_t)rounded;
        memcpy(p, &v, sizeof v);
        break;
    }
    case X3PIO_TYPE_UINT16:
    {
        const uint16_t v = (uint16_t)rounded;
        memcpy(p, &v, sizeof v);
        break;
    }
    case X3PIO_TYPE_INT32:
    {
        const int32_t v = (int32_t)rounded;
        memcpy(p, &v, sizeof v);
        break;
    }
    case X3PIO_TYPE_UINT32:
    {
        const uint32_t v = (uint32_t)rounded;
        memcpy(p, &v, sizeof v);
        break;
    }
    default:
        break;
    }
    return true;
}

bool x3pio_matrix_init(x3pio_matrix *matrix, const void *data,
                       const x3pio_layout *layout, size_t columns, size_t rows,
                       bool writable)
{
    const size_t size = x3pio_type_size(layout->type);
    if (size == 0 || data == NULL || columns == 0 || rows == 0)
    {
        return false;
    }
    matrix->base.read = data;
    matrix->type = layout->type;
    matrix->columns = columns;
    matrix->element_stride =
        layout->element_stride != 0 ? layout->element_stride : (ptrdiff_t)size;
    /* The rows follow each other if the layout says nothing. */
    if (layout->row_stride != 0)
    {
        matrix->row_stride = layout->row_stride;
    }
    else
    {
        size_t row_bytes = 0;
        if (!x3pio_multiply(columns, (size_t)matrix->element_stride, &row_bytes)
            || row_bytes > (size_t)PTRDIFF_MAX || matrix->element_stride < 0)
        {
            return false;
        }
        matrix->row_stride = (ptrdiff_t)row_bytes;
    }
    (void)writable;
    return matrix->element_stride != 0 && matrix->row_stride != 0;
}

/* Walks over the elements row after row. `base` is the first element, as the
 * type of the pointer `at_` that the body gets. */
#define WALK(matrix, base, pointer_type, first, count, body)                   \
    do                                                                         \
    {                                                                          \
        size_t row_ = (first) / (matrix)->columns;                             \
        size_t column_ = (first) % (matrix)->columns;                          \
        pointer_type at_ = (base) + (ptrdiff_t)row_ * (matrix)->row_stride     \
                           + (ptrdiff_t)column_ * (matrix)->element_stride;    \
        for (size_t k = 0; k < (count); ++k)                                   \
        {                                                                      \
            body;                                                              \
            if (++column_ < (matrix)->columns)                                 \
            {                                                                  \
                at_ += (matrix)->element_stride;                               \
            }                                                                  \
            else                                                               \
            {                                                                  \
                column_ = 0;                                                   \
                ++row_;                                                        \
                at_ = (base) + (ptrdiff_t)row_ * (matrix)->row_stride;         \
            }                                                                  \
        }                                                                      \
    } while (0)

void x3pio_matrix_load(const x3pio_matrix *matrix, size_t first, double *out,
                       size_t count)
{
    WALK(matrix, matrix->base.read, const unsigned char *, first, count,
         out[k] = load_element(matrix->type, at_));
}

x3pio_status x3pio_matrix_store(const x3pio_matrix *matrix, size_t first,
                                const double *values, size_t count)
{
    x3pio_status status = X3PIO_OK;
    WALK(
        matrix, matrix->base.write, unsigned char *, first, count,
        if (!store_element(matrix->type, at_, values[k])) {
            status = X3PIO_ERR_RANGE;
            break;
        });
    return status;
}
