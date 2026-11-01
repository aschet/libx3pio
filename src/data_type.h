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
 * and a finite number must not exceed the range of a float.
 */
bool x3pio_fits_float(double value);

#endif /* X3PIO_SRC_DATA_TYPE_H */
