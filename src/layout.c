/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Layouts, fields, and the bits of a mask.
 */

#include "common.h"

void x3pio_layout_init(x3pio_layout *layout)
{
    *layout = (x3pio_layout){X3PIO_TYPE_DOUBLE, 0, 0};
}

const x3pio_layout *x3pio_layout_packed(x3pio_type type)
{
    static const x3pio_layout layouts[] = {
        {X3PIO_TYPE_INT8, 0, 0},  {X3PIO_TYPE_UINT8, 0, 0},
        {X3PIO_TYPE_INT16, 0, 0}, {X3PIO_TYPE_UINT16, 0, 0},
        {X3PIO_TYPE_INT32, 0, 0}, {X3PIO_TYPE_UINT32, 0, 0},
        {X3PIO_TYPE_FLOAT, 0, 0}, {X3PIO_TYPE_DOUBLE, 0, 0}};
    if (x3pio_type_size(type) == 0)
    {
        return NULL;
    }
    return &layouts[type];
}

void x3pio_fields_init(x3pio_fields *fields)
{
    *fields = (x3pio_fields){0};
}

void x3pio_const_fields_init(x3pio_const_fields *fields)
{
    *fields = (x3pio_const_fields){0};
}

void x3pio_validity_pack(const unsigned char *flags, size_t count,
                         unsigned char *bits)
{
    memset(bits, 0, (count + 7) / 8);
    for (size_t j = 0; j < count; ++j)
    {
        if (flags[j] != 0)
        {
            bits[j / 8] = (unsigned char)(bits[j / 8] | (1U << (j % 8)));
        }
    }
}

void x3pio_validity_unpack(const unsigned char *bits, size_t count,
                           unsigned char *flags)
{
    for (size_t j = 0; j < count; ++j)
    {
        flags[j] = (unsigned char)((bits[j / 8] >> (j % 8)) & 1U);
    }
}
