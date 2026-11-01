/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The facts about the data types of values: sizes, ranges and the
 * conversion of a number to a type.
 */

#include "data_type.h"
#include <float.h>
#include <math.h>

size_t x3pio_type_size(x3pio_type type)
{
    switch (type)
    {
    case X3PIO_TYPE_INT8:
    case X3PIO_TYPE_UINT8:
        return 1;
    case X3PIO_TYPE_INT16:
    case X3PIO_TYPE_UINT16:
        return 2;
    case X3PIO_TYPE_INT32:
    case X3PIO_TYPE_UINT32:
    case X3PIO_TYPE_FLOAT:
        return 4;
    case X3PIO_TYPE_DOUBLE:
        return 8;
    }
    return 0;
}

bool x3pio_type_is_stored(x3pio_type type)
{
    return type == X3PIO_TYPE_INT16 || type == X3PIO_TYPE_INT32
           || type == X3PIO_TYPE_FLOAT || type == X3PIO_TYPE_DOUBLE;
}

/* The bounds of the integer types, as doubles. */
#define BOUNDS(type, lowest, highest)                                          \
    case type:                                                                 \
        *low = (lowest);                                                       \
        *high = (highest);                                                     \
        return true

bool x3pio_integer_range(x3pio_type type, double *low, double *high)
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

bool x3pio_round_to_integer(x3pio_type type, double value, double *out)
{
    double low = 0;
    double high = 0;
    return x3pio_integer_range(type, &low, &high)
           && x3pio_round_in_range(value, low, high, out);
}
