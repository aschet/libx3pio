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

#ifndef X3PIO_SRC_DATA_TYPE_H
#define X3PIO_SRC_DATA_TYPE_H

#include "common.h"
#include <float.h>
#include <math.h>

/**
 * Gives the range of an integer type.
 *
 * @param type The type.
 * @param[out] low Receives the lowest value.
 * @param[out] high Receives the highest value.
 * @return False if the type is no integer type.
 */
bool x3pio_integer_range(x3pio_type type, double *low, double *high);

/**
 * Converts a number to the nearest integer of a type, a tie to the even one. A
 * NaN becomes 0.
 *
 * @param type An integer type.
 * @param value The number.
 * @param[out] out Receives the integer as a double.
 * @return False if the type is no integer type, or the number does not fit it.
 */
bool x3pio_round_to_integer(x3pio_type type, double value, double *out);

/**
 * Tells whether a number can be stored as a float. A NaN and an infinity can,
 * and a finite number must not exceed the range of a float. The function is
 * inline, since it is called for every value that is converted.
 */
static inline bool x3pio_fits_float(double value)
{
    return !(isfinite(value) && fabs(value) > (double)FLT_MAX);
}

/**
 * Converts a number to the nearest integer in a range, a tie to the even one. A
 * NaN becomes 0. The function is inline, since it is called for every value
 * that is converted.
 *
 * @param value The number.
 * @param low The lowest integer.
 * @param high The highest integer.
 * @param[out] out Receives the integer as a double.
 * @return False if the number does not fit the range.
 */
static inline bool x3pio_round_in_range(double value, double low, double high,
                                        double *out)
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

#endif /* X3PIO_SRC_DATA_TYPE_H */
