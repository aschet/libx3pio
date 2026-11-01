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

x3pio_compression x3pio_compression_default(void)
{
    return (x3pio_compression){X3PIO_COMPRESS_DEFLATE, -1};
}

x3pio_transformation x3pio_transformation_identity(void)
{
    return (x3pio_transformation){{1, 0, 0, 0, 1, 0, 0, 0, 1}, {0, 0, 0}};
}

void x3pio_transformation_init(x3pio_transformation *transformation)
{
    if (transformation != NULL)
    {
        *transformation = x3pio_transformation_identity();
    }
}

void x3pio_compression_init(x3pio_compression *compression)
{
    if (compression != NULL)
    {
        *compression = x3pio_compression_default();
    }
}

bool x3pio_transformation_has_rotation(
    const x3pio_transformation *transformation)
{
    static const double identity[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    for (size_t i = 0; i < 9; ++i)
    {
        if (transformation->rotation[i] != identity[i])
        {
            return true;
        }
    }
    return false;
}

void x3pio_transformation_to_global(const x3pio_transformation *transformation,
                                    const double v[3], double g[3])
{
    const double a = v[0];
    const double b = v[1];
    const double c = v[2];
    g[0] = transformation->rotation[0] * a + transformation->rotation[1] * b
           + transformation->rotation[2] * c + transformation->offset[0];
    g[1] = transformation->rotation[3] * a + transformation->rotation[4] * b
           + transformation->rotation[5] * c + transformation->offset[1];
    g[2] = transformation->rotation[6] * a + transformation->rotation[7] * b
           + transformation->rotation[8] * c + transformation->offset[2];
}

void x3pio_transformation_to_view(const x3pio_transformation *transformation,
                                  const double g[3], double v[3])
{
    const double a = g[0] - transformation->offset[0];
    const double b = g[1] - transformation->offset[1];
    const double c = g[2] - transformation->offset[2];
    /* The inverse of a rotation is its transpose. */
    v[0] = transformation->rotation[0] * a + transformation->rotation[3] * b
           + transformation->rotation[6] * c;
    v[1] = transformation->rotation[1] * a + transformation->rotation[4] * b
           + transformation->rotation[7] * c;
    v[2] = transformation->rotation[2] * a + transformation->rotation[5] * b
           + transformation->rotation[8] * c;
}

const char *x3pio_version(void)
{
    return X3PIO_VERSION_STRING;
}
