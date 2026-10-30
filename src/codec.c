/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Encoding and decoding of the raw values of a file, and the transfer of
 * values from and to the views of the program.
 */

#include "codec.h"
#include <float.h>
#include <math.h>
#include <stddef.h>

/* ---- the little-endian values of a file ------------------------------- */

static uint32_t load_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) | ((uint32_t)p[2] << 16U)
           | ((uint32_t)p[3] << 24U);
}

static uint64_t load_u64(const unsigned char *p)
{
    return (uint64_t)load_u32(p) | ((uint64_t)load_u32(p + 4) << 32U);
}

static void store_u32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v & 0xFFU);
    p[1] = (unsigned char)((v >> 8U) & 0xFFU);
    p[2] = (unsigned char)((v >> 16U) & 0xFFU);
    p[3] = (unsigned char)((v >> 24U) & 0xFFU);
}

static void store_u64(unsigned char *p, uint64_t v)
{
    store_u32(p, (uint32_t)(v & 0xFFFFFFFFU));
    store_u32(p + 4, (uint32_t)(v >> 32U));
}

/* The number that a pattern of bits stands for. */
static float float_of_bits(uint32_t bits)
{
    float value = 0;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static double double_of_bits(uint64_t bits)
{
    double value = 0;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static uint32_t bits_of_float(float value)
{
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static uint64_t bits_of_double(double value)
{
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

/* ---- conversion of a double to a type ---------------------------------- */

/* Converts a double to an integer with rounding to nearest, ties to even. A NaN
 * becomes 0. */
static bool to_integer(double value, double low, double high, double *out)
{
    if (isnan(value))
    {
        *out = 0.0;
        return true;
    }
    const double rounded = nearbyint(value);
    if (!(rounded >= low && rounded <= high))
    {
        return false;
    }
    *out = rounded;
    return true;
}

/* The bounds of the integer types, as doubles. */
#define BOUNDS(type, low, high)                                                \
    case type:                                                                 \
        *l = (low);                                                            \
        *h = (high);                                                           \
        return true

static bool integer_bounds(x3pio_type type, double *l, double *h)
{
    switch (type)
    {
        BOUNDS(X3PIO_TYPE_INT8, -128.0, 127.0);
        BOUNDS(X3PIO_TYPE_UINT8, 0.0, 255.0);
        BOUNDS(X3PIO_TYPE_INT16, -32768.0, 32767.0);
        BOUNDS(X3PIO_TYPE_UINT16, 0.0, 65535.0);
        BOUNDS(X3PIO_TYPE_INT32, -2147483648.0, 2147483647.0);
        BOUNDS(X3PIO_TYPE_UINT32, 0.0, 4294967295.0);
    case X3PIO_TYPE_FLOAT:
    case X3PIO_TYPE_DOUBLE:
        break;
    }
    return false;
}

/* ---- the values of a file ---------------------------------------------- */

void x3pio_decode_samples(x3pio_type type, const unsigned char *src,
                          size_t stride, double *out, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        const unsigned char *p = src + i * stride;
        switch (type)
        {
        case X3PIO_TYPE_INT16:
            out[i] =
                (double)(int16_t)(uint16_t)((uint16_t)p[0]
                                            | (uint16_t)((uint16_t)p[1] << 8U));
            break;
        case X3PIO_TYPE_INT32:
            out[i] = (double)(int32_t)load_u32(p);
            break;
        case X3PIO_TYPE_FLOAT:
            out[i] = (double)float_of_bits(load_u32(p));
            break;
        case X3PIO_TYPE_DOUBLE:
            out[i] = double_of_bits(load_u64(p));
            break;
        default:
            out[i] = 0.0;
            break;
        }
    }
}

x3pio_status x3pio_encode_samples(x3pio_type type, const double *values,
                                  size_t count, unsigned char *dst,
                                  size_t stride)
{
    for (size_t i = 0; i < count; ++i)
    {
        unsigned char *p = dst + i * stride;
        const double value = values[i];
        switch (type)
        {
        case X3PIO_TYPE_INT16:
        {
            double r = 0;
            if (!to_integer(value, -32768.0, 32767.0, &r))
            {
                return X3PIO_ERR_RANGE;
            }
            const uint16_t v = (uint16_t)(int16_t)r;
            p[0] = (unsigned char)(v & 0xFFU);
            p[1] = (unsigned char)(v >> 8U);
            break;
        }
        case X3PIO_TYPE_INT32:
        {
            double r = 0;
            if (!to_integer(value, -2147483648.0, 2147483647.0, &r))
            {
                return X3PIO_ERR_RANGE;
            }
            store_u32(p, (uint32_t)(int32_t)r);
            break;
        }
        case X3PIO_TYPE_FLOAT:
            if (isfinite(value) && fabs(value) > (double)FLT_MAX)
            {
                return X3PIO_ERR_RANGE;
            }
            store_u32(p, bits_of_float((float)value));
            break;
        case X3PIO_TYPE_DOUBLE:
            store_u64(p, bits_of_double(value));
            break;
        default:
            return X3PIO_ERR_ARGUMENT;
        }
    }
    return X3PIO_OK;
}

/* ---- the elements of a view -------------------------------------------- */

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
        if (isfinite(value) && fabs(value) > (double)FLT_MAX)
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
    double low = 0;
    double high = 0;
    double rounded = 0;
    if (!integer_bounds(type, &low, &high)
        || !to_integer(value, low, high, &rounded))
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

bool x3pio_matrix_make(x3pio_matrix *matrix, const void *data,
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
