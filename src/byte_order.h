/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Conversion between the little-endian byte order of files and the
 * numbers of the host.
 *
 * The values of a file are little endian. The functions compose and split the
 * bytes with shifts, so they work on a host of either byte order, and they do
 * not read memory as a wider type, so alignment does not matter.
 */

#ifndef X3PIO_SRC_BYTE_ORDER_H
#define X3PIO_SRC_BYTE_ORDER_H

#include <stdint.h>

/** Reads a little-endian 16-bit integer. */
uint16_t x3pio_load_u16(const unsigned char *p);

/** Reads a little-endian 32-bit integer. */
uint32_t x3pio_load_u32(const unsigned char *p);

/** Reads a little-endian 64-bit integer. */
uint64_t x3pio_load_u64(const unsigned char *p);

/** Reads a little-endian IEEE 754 single precision number. */
float x3pio_load_float(const unsigned char *p);

/** Reads a little-endian IEEE 754 double precision number. */
double x3pio_load_double(const unsigned char *p);

/** Writes a 16-bit integer as little-endian bytes. */
void x3pio_store_u16(unsigned char *p, uint16_t value);

/** Writes a 32-bit integer as little-endian bytes. */
void x3pio_store_u32(unsigned char *p, uint32_t value);

/** Writes a 64-bit integer as little-endian bytes. */
void x3pio_store_u64(unsigned char *p, uint64_t value);

/** Writes a single precision number as little-endian bytes. */
void x3pio_store_float(unsigned char *p, float value);

/** Writes a double precision number as little-endian bytes. */
void x3pio_store_double(unsigned char *p, double value);

#endif /* X3PIO_SRC_BYTE_ORDER_H */
