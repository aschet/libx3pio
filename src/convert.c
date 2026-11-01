/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Conversion between stored values and metres.
 */

#include "document.h"
#include "matrix.h"
#include "samples.h"
#include <math.h>

/* The values that are converted at a time. */
#define BLOCK 256

static bool metres_type(x3pio_type type)
{
    return type == X3PIO_TYPE_FLOAT || type == X3PIO_TYPE_DOUBLE;
}

x3pio_status x3pio_decode_values(const x3pio_document *document,
                                 x3pio_coordinate coordinate,
                                 const void *stored, size_t count,
                                 const x3pio_layout *layout, void *metres)
{
    if (document == NULL || stored == NULL || layout == NULL || metres == NULL
        || (unsigned)coordinate > 2U
        || document->axes[coordinate].type != X3PIO_AXIS_ABSOLUTE
        || !metres_type(layout->type))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (count == 0)
    {
        return X3PIO_OK;
    }
    x3pio_matrix out;
    if (!x3pio_matrix_init(&out, metres, layout, count, 1, true))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    const x3pio_axis *axis = &document->axes[coordinate];
    const size_t size = x3pio_type_size(axis->storage);
    double block[BLOCK];
    for (size_t at = 0; at < count; at += BLOCK)
    {
        const size_t n = count - at < BLOCK ? count - at : BLOCK;
        x3pio_decode_samples(axis->storage,
                             (const unsigned char *)stored + at * size, size,
                             block, n);
        for (size_t i = 0; i < n; ++i)
        {
            block[i] *= axis->increment;
        }
        const x3pio_status status = x3pio_matrix_store(&out, at, block, n);
        if (status != X3PIO_OK)
        {
            return status;
        }
    }
    return X3PIO_OK;
}

x3pio_status x3pio_encode_values(const x3pio_document *document,
                                 x3pio_coordinate coordinate,
                                 const void *metres, size_t count,
                                 const x3pio_layout *layout, void *stored)
{
    if (document == NULL || stored == NULL || layout == NULL || metres == NULL
        || (unsigned)coordinate > 2U
        || document->axes[coordinate].type != X3PIO_AXIS_ABSOLUTE
        || !metres_type(layout->type)
        || !(document->axes[coordinate].increment > 0.0))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (count == 0)
    {
        return X3PIO_OK;
    }
    x3pio_matrix in;
    if (!x3pio_matrix_init(&in, metres, layout, count, 1, false))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    const x3pio_axis *axis = &document->axes[coordinate];
    const size_t size = x3pio_type_size(axis->storage);
    double block[BLOCK];
    for (size_t at = 0; at < count; at += BLOCK)
    {
        const size_t n = count - at < BLOCK ? count - at : BLOCK;
        x3pio_matrix_load(&in, at, block, n);
        for (size_t i = 0; i < n; ++i)
        {
            block[i] /= axis->increment;
        }
        const x3pio_status status = x3pio_encode_samples(
            axis->storage, block, n, (unsigned char *)stored + at * size, size);
        if (status != X3PIO_OK)
        {
            return status;
        }
    }
    return X3PIO_OK;
}

double x3pio_suggest_increment_for_range(x3pio_type type, double lowest,
                                         double highest)
{
    double range = 0.0;
    if (type == X3PIO_TYPE_INT16)
    {
        range = 32767.0;
    }
    else if (type == X3PIO_TYPE_INT32)
    {
        range = 2147483647.0;
    }
    else
    {
        return 1.0;
    }
    double largest =
        fabs(lowest) > fabs(highest) ? fabs(lowest) : fabs(highest);
    if (!isfinite(largest))
    {
        return 1.0;
    }
    const double step = largest / range;
    return (step > 0.0 && isfinite(step)) ? step : 1.0;
}

x3pio_status x3pio_suggest_increment(x3pio_type type,
                                     const x3pio_layout *layout, const void *z,
                                     size_t rows, size_t columns,
                                     double *increment)
{
    if (layout == NULL || z == NULL || increment == NULL
        || !metres_type(layout->type))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    double lowest = INFINITY;
    double highest = -INFINITY;
    if (rows > 0 && columns > 0)
    {
        x3pio_matrix in;
        if (!x3pio_matrix_init(&in, z, layout, columns, rows, false))
        {
            return X3PIO_ERR_ARGUMENT;
        }
        size_t total = 0;
        if (!x3pio_multiply(rows, columns, &total))
        {
            return X3PIO_ERR_ARGUMENT;
        }
        double block[BLOCK];
        for (size_t at = 0; at < total; at += BLOCK)
        {
            const size_t n = total - at < BLOCK ? total - at : BLOCK;
            x3pio_matrix_load(&in, at, block, n);
            for (size_t i = 0; i < n; ++i)
            {
                if (isfinite(block[i]))
                {
                    lowest = block[i] < lowest ? block[i] : lowest;
                    highest = block[i] > highest ? block[i] : highest;
                }
            }
        }
    }
    *increment = lowest <= highest
                     ? x3pio_suggest_increment_for_range(type, lowest, highest)
                     : 1.0;
    return X3PIO_OK;
}
