/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Texts of the status codes, sizes of the types, and the initial values
 * of the plain structures.
 */

#include "common.h"
#include <limits.h>
#include <math.h>

/* The format counts in 8-bit bytes and stores IEEE 754 numbers. */
_Static_assert(CHAR_BIT == 8, "the format counts in 8-bit bytes");
_Static_assert(sizeof(float) == 4, "float has to be binary32");
_Static_assert(sizeof(double) == 8, "double has to be binary64");

const char *x3pio_status_string(x3pio_status status)
{
    switch (status)
    {
    case X3PIO_OK:
        return "no error";
    case X3PIO_ERR_ARGUMENT:
        return "invalid argument";
    case X3PIO_ERR_STATE:
        return "the call does not fit the state of the handle";
    case X3PIO_ERR_IO:
        return "input or output error";
    case X3PIO_ERR_FORMAT:
        return "not an x3p file, or a damaged one";
    case X3PIO_ERR_UNSUPPORTED:
        return "a feature of the file is not supported";
    case X3PIO_ERR_RANGE:
        return "a number does not fit its type";
    case X3PIO_ERR_CHECKSUM:
        return "a checksum does not match the data";
    case X3PIO_ERR_MEMORY:
        return "out of memory";
    }
    return "unknown status";
}

size_t x3pio_type_size(x3pio_type type)
{
    switch (type)
    {
    case X3PIO_TYPE_INT8:
    case X3PIO_TYPE_UINT8:
        return 1;
    case X3PIO_TYPE_INT16:
    case X3PIO_TYPE_UINT16:
        return 2;
    case X3PIO_TYPE_INT32:
    case X3PIO_TYPE_UINT32:
    case X3PIO_TYPE_FLOAT:
        return 4;
    case X3PIO_TYPE_DOUBLE:
        return 8;
    }
    return 0;
}

bool x3pio_type_is_stored(x3pio_type type)
{
    return type == X3PIO_TYPE_INT16 || type == X3PIO_TYPE_INT32
           || type == X3PIO_TYPE_FLOAT || type == X3PIO_TYPE_DOUBLE;
}

x3pio_compression x3pio_compression_default(void)
{
    return (x3pio_compression){X3PIO_COMPRESS_DEFLATE, -1};
}

x3pio_placement x3pio_placement_identity(void)
{
    return (x3pio_placement){{1, 0, 0, 0, 1, 0, 0, 0, 1}, {0, 0, 0}};
}

void x3pio_placement_init(x3pio_placement *placement)
{
    if (placement != NULL)
    {
        *placement = x3pio_placement_identity();
    }
}

void x3pio_compression_init(x3pio_compression *compression)
{
    if (compression != NULL)
    {
        *compression = x3pio_compression_default();
    }
}

bool x3pio_placement_has_rotation(const x3pio_placement *placement)
{
    static const double identity[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    for (size_t i = 0; i < 9; ++i)
    {
        if (placement->rotation[i] != identity[i])
        {
            return true;
        }
    }
    return false;
}

void x3pio_placement_to_global(const x3pio_placement *p, const double v[3],
                               double g[3])
{
    const double a = v[0];
    const double b = v[1];
    const double c = v[2];
    g[0] = p->rotation[0] * a + p->rotation[1] * b + p->rotation[2] * c
           + p->offset[0];
    g[1] = p->rotation[3] * a + p->rotation[4] * b + p->rotation[5] * c
           + p->offset[1];
    g[2] = p->rotation[6] * a + p->rotation[7] * b + p->rotation[8] * c
           + p->offset[2];
}

void x3pio_placement_to_view(const x3pio_placement *p, const double g[3],
                             double v[3])
{
    const double a = g[0] - p->offset[0];
    const double b = g[1] - p->offset[1];
    const double c = g[2] - p->offset[2];
    /* The inverse of a rotation is its transpose. */
    v[0] = p->rotation[0] * a + p->rotation[3] * b + p->rotation[6] * c;
    v[1] = p->rotation[1] * a + p->rotation[4] * b + p->rotation[7] * c;
    v[2] = p->rotation[2] * a + p->rotation[5] * b + p->rotation[8] * c;
}

const char *x3pio_version(void)
{
    return X3PIO_VERSION_STRING;
}
