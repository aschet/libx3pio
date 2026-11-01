/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The swap of the bytes of values, for a host that is not little endian.
 */

#include "byte_order.h"
#include <string.h>

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
