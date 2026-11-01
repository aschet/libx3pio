/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Conversion between the little-endian byte order of files and that of
 * the host.
 *
 * The functions that a value passes through are inline: they are called for
 * every value of a file, and on a little-endian host they compile to a plain
 * load or store. Only the swap is a function of its own.
 */

#ifndef X3PIO_SRC_BYTE_ORDER_H
#define X3PIO_SRC_BYTE_ORDER_H

#include "common.h"

/** Tells whether the machine stores the least significant byte first. */
static inline bool x3pio_host_is_little_endian(void)
{
    const uint16_t one = 1;
    unsigned char first = 0;
    memcpy(&first, &one, 1);
    return first == 1;
}

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
static inline void x3pio_convert_byte_order(void *values, size_t count,
                                            x3pio_type type)
{
    if (!x3pio_host_is_little_endian())
    {
        x3pio_swap_bytes(values, count, x3pio_type_size(type));
    }
}

/* The values of a file are moved as bytes of the host and converted. */

/** Reads a little-endian 16-bit integer. */
static inline uint16_t x3pio_load_u16(const unsigned char *p)
{
    uint16_t value;
    memcpy(&value, p, sizeof value);
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_UINT16);
    return value;
}

/** Reads a little-endian 32-bit integer. */
static inline uint32_t x3pio_load_u32(const unsigned char *p)
{
    uint32_t value;
    memcpy(&value, p, sizeof value);
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_UINT32);
    return value;
}

/** Reads a little-endian IEEE 754 single precision number. */
static inline float x3pio_load_float(const unsigned char *p)
{
    float value;
    memcpy(&value, p, sizeof value);
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_FLOAT);
    return value;
}

/** Reads a little-endian IEEE 754 double precision number. */
static inline double x3pio_load_double(const unsigned char *p)
{
    double value;
    memcpy(&value, p, sizeof value);
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_DOUBLE);
    return value;
}

/** Writes a 16-bit integer as little-endian bytes. */
static inline void x3pio_store_u16(unsigned char *p, uint16_t value)
{
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_UINT16);
    memcpy(p, &value, sizeof value);
}

/** Writes a 32-bit integer as little-endian bytes. */
static inline void x3pio_store_u32(unsigned char *p, uint32_t value)
{
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_UINT32);
    memcpy(p, &value, sizeof value);
}

/** Writes a single precision number as little-endian bytes. */
static inline void x3pio_store_float(unsigned char *p, float value)
{
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_FLOAT);
    memcpy(p, &value, sizeof value);
}

/** Writes a double precision number as little-endian bytes. */
static inline void x3pio_store_double(unsigned char *p, double value)
{
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_DOUBLE);
    memcpy(p, &value, sizeof value);
}

#endif /* X3PIO_SRC_BYTE_ORDER_H */
