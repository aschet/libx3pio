/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Conversion between the little-endian byte order of files and that of
 * the host.
 */

#ifndef X3PIO_SRC_BYTE_ORDER_H
#define X3PIO_SRC_BYTE_ORDER_H

#include "common.h"

/** Tells whether the machine stores the least significant byte first. */
bool x3pio_host_is_little_endian(void);

/**
 * Reverses the bytes of each of `count` values of `width` bytes, in place,
 * whatever the byte order of the host. The values may lie at any address.
 *
 * @param[in,out] values The values.
 * @param count The number of values.
 * @param width The width of a value in bytes: 2, 4 or 8. Another width, such as
 * 1, leaves the values as they are.
 */
void x3pio_swap_bytes(void *values, size_t count, size_t width);

/**
 * Converts values between the byte order of the host and the file order
 * (little endian), in place. The conversion is the same in both directions. On
 * a little-endian host it does nothing. A value of one byte is left as it is.
 *
 * @param[in,out] values The values.
 * @param count The number of values.
 * @param type The type of the values.
 */
void x3pio_convert_byte_order(void *values, size_t count, x3pio_type type);

/** Reads a little-endian 16-bit integer. */
uint16_t x3pio_load_u16(const unsigned char *p);

/** Reads a little-endian 32-bit integer. */
uint32_t x3pio_load_u32(const unsigned char *p);

/** Reads a little-endian IEEE 754 single precision number. */
float x3pio_load_float(const unsigned char *p);

/** Reads a little-endian IEEE 754 double precision number. */
double x3pio_load_double(const unsigned char *p);

/** Writes a 16-bit integer as little-endian bytes. */
void x3pio_store_u16(unsigned char *p, uint16_t value);

/** Writes a 32-bit integer as little-endian bytes. */
void x3pio_store_u32(unsigned char *p, uint32_t value);

/** Writes a single precision number as little-endian bytes. */
void x3pio_store_float(unsigned char *p, float value);

/** Writes a double precision number as little-endian bytes. */
void x3pio_store_double(unsigned char *p, double value);

#endif /* X3PIO_SRC_BYTE_ORDER_H */
