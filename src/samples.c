/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The values of a file: little-endian numbers of a stored type, and
 * their conversion from and to doubles.
 */

#include "samples.h"
#include "byte_order.h"
#include "data_type.h"

void x3pio_decode_samples(x3pio_type type, const unsigned char *src,
                          size_t stride, double *out, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        const unsigned char *p = src + i * stride;
        switch (type)
        {
        case X3PIO_TYPE_INT16:
            out[i] = (double)(int16_t)x3pio_load_u16(p);
            break;
        case X3PIO_TYPE_INT32:
            out[i] = (double)(int32_t)x3pio_load_u32(p);
            break;
        case X3PIO_TYPE_FLOAT:
            out[i] = (double)x3pio_load_float(p);
            break;
        case X3PIO_TYPE_DOUBLE:
            out[i] = x3pio_load_double(p);
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
            if (!x3pio_round_to_integer(type, value, &r))
            {
                return X3PIO_ERR_RANGE;
            }
            x3pio_store_u16(p, (uint16_t)(int16_t)r);
            break;
        }
        case X3PIO_TYPE_INT32:
        {
            double r = 0;
            if (!x3pio_round_to_integer(type, value, &r))
            {
                return X3PIO_ERR_RANGE;
            }
            x3pio_store_u32(p, (uint32_t)(int32_t)r);
            break;
        }
        case X3PIO_TYPE_FLOAT:
            if (!x3pio_fits_float(value))
            {
                return X3PIO_ERR_RANGE;
            }
            x3pio_store_float(p, (float)value);
            break;
        case X3PIO_TYPE_DOUBLE:
            x3pio_store_double(p, value);
            break;
        default:
            return X3PIO_ERR_ARGUMENT;
        }
    }
    return X3PIO_OK;
}
