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

#include "byte_order.h"
#include <string.h>

bool x3pio_host_is_little_endian(void)
{
    const uint16_t one = 1;
    unsigned char first = 0;
    memcpy(&first, &one, 1);
    return first == 1;
}

/* A byte swap written with shifts, which compilers translate to the byte swap
 * instruction of the processor, where there is one. */
static uint16_t swap16(uint16_t value)
{
    return (uint16_t)((value << 8) | (value >> 8));
}

static uint32_t swap32(uint32_t value)
{
    return (value << 24) | ((value & 0xFF00U) << 8) | ((value >> 8) & 0xFF00U)
           | (value >> 24);
}

static uint64_t swap64(uint64_t value)
{
    return ((uint64_t)swap32((uint32_t)value) << 32)
           | swap32((uint32_t)(value >> 32));
}

/*
 * Defines swap_values<bits>(), which reverses the bytes of each value of a
 * width. The values are moved through memcpy, so that their alignment in memory
 * does not matter.
 */
#define SWAP_VALUES(bits)                                                      \
    static void swap_values##bits(unsigned char *bytes, size_t count)          \
    {                                                                          \
        for (size_t i = 0; i < count; ++i)                                     \
        {                                                                      \
            uint##bits##_t value;                                              \
            memcpy(&value, bytes + i * sizeof value, sizeof value);            \
            value = swap##bits(value);                                         \
            memcpy(bytes + i * sizeof value, &value, sizeof value);            \
        }                                                                      \
    }

SWAP_VALUES(16)
SWAP_VALUES(32)
SWAP_VALUES(64)

#undef SWAP_VALUES

void x3pio_swap_bytes(void *values, size_t count, size_t width)
{
    switch (width)
    {
    case 2:
        swap_values16(values, count);
        break;
    case 4:
        swap_values32(values, count);
        break;
    case 8:
        swap_values64(values, count);
        break;
    default:
        break;
    }
}

void x3pio_convert_byte_order(void *values, size_t count, x3pio_type type)
{
    if (x3pio_host_is_little_endian())
    {
        return;
    }
    x3pio_swap_bytes(values, count, x3pio_type_size(type));
}

/* The values of a file are read as bytes of the host and converted. */
uint16_t x3pio_load_u16(const unsigned char *p)
{
    uint16_t value;
    memcpy(&value, p, sizeof value);
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_UINT16);
    return value;
}

uint32_t x3pio_load_u32(const unsigned char *p)
{
    uint32_t value;
    memcpy(&value, p, sizeof value);
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_UINT32);
    return value;
}

float x3pio_load_float(const unsigned char *p)
{
    float value;
    memcpy(&value, p, sizeof value);
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_FLOAT);
    return value;
}

double x3pio_load_double(const unsigned char *p)
{
    double value;
    memcpy(&value, p, sizeof value);
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_DOUBLE);
    return value;
}

void x3pio_store_u16(unsigned char *p, uint16_t value)
{
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_UINT16);
    memcpy(p, &value, sizeof value);
}

void x3pio_store_u32(unsigned char *p, uint32_t value)
{
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_UINT32);
    memcpy(p, &value, sizeof value);
}

void x3pio_store_float(unsigned char *p, float value)
{
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_FLOAT);
    memcpy(p, &value, sizeof value);
}

void x3pio_store_double(unsigned char *p, double value)
{
    x3pio_convert_byte_order(&value, 1, X3PIO_TYPE_DOUBLE);
    memcpy(p, &value, sizeof value);
}
