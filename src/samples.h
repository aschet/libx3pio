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

#ifndef X3PIO_SRC_SAMPLES_H
#define X3PIO_SRC_SAMPLES_H

#include "common.h"

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

#endif /* X3PIO_SRC_SAMPLES_H */
