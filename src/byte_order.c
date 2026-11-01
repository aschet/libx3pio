/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Conversion between the little-endian byte order of files and the
 * numbers of the host.
 */

#include "byte_order.h"
#include <string.h>

uint16_t x3pio_load_u16(const unsigned char *p)
{
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8U));
}

uint32_t x3pio_load_u32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8U) | ((uint32_t)p[2] << 16U)
           | ((uint32_t)p[3] << 24U);
}

uint64_t x3pio_load_u64(const unsigned char *p)
{
    return (uint64_t)x3pio_load_u32(p)
           | ((uint64_t)x3pio_load_u32(p + 4) << 32U);
}

void x3pio_store_u16(unsigned char *p, uint16_t value)
{
    p[0] = (unsigned char)(value & 0xFFU);
    p[1] = (unsigned char)((value >> 8U) & 0xFFU);
}

void x3pio_store_u32(unsigned char *p, uint32_t value)
{
    p[0] = (unsigned char)(value & 0xFFU);
    p[1] = (unsigned char)((value >> 8U) & 0xFFU);
    p[2] = (unsigned char)((value >> 16U) & 0xFFU);
    p[3] = (unsigned char)((value >> 24U) & 0xFFU);
}

void x3pio_store_u64(unsigned char *p, uint64_t value)
{
    x3pio_store_u32(p, (uint32_t)(value & 0xFFFFFFFFU));
    x3pio_store_u32(p + 4, (uint32_t)(value >> 32U));
}

/* The number that a pattern of bits stands for, and the pattern of a number. A
 * copy of the bytes is the defined way to reinterpret them. */
float x3pio_load_float(const unsigned char *p)
{
    const uint32_t bits = x3pio_load_u32(p);
    float value = 0;
    memcpy(&value, &bits, sizeof value);
    return value;
}

double x3pio_load_double(const unsigned char *p)
{
    const uint64_t bits = x3pio_load_u64(p);
    double value = 0;
    memcpy(&value, &bits, sizeof value);
    return value;
}

void x3pio_store_float(unsigned char *p, float value)
{
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    x3pio_store_u32(p, bits);
}

void x3pio_store_double(unsigned char *p, double value)
{
    uint64_t bits = 0;
    memcpy(&bits, &value, sizeof bits);
    x3pio_store_u64(p, bits);
}
