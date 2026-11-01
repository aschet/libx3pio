/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The checks of a document: the values of its fields and the whole.
 */

#include "allocator.h"
#include "document.h"
#include <limits.h>
#include <math.h>

/* ---- checks ----------------------------------------------------------- */

static bool rotation_valid(const x3pio_transformation *p, const char **message)
{
    const double tolerance = 1e-6;
    const double *r = p->rotation;
    for (size_t i = 0; i < 9; ++i)
    {
        if (!isfinite(r[i]) || r[i] < -1.0 - tolerance
            || r[i] > 1.0 + tolerance)
        {
            *message = "The rotation must be finite and within [-1, 1]";
            return false;
        }
    }
    for (size_t i = 0; i < 3; ++i)
    {
        for (size_t j = 0; j < 3; ++j)
        {
            double dot = 0.0;
            for (size_t k = 0; k < 3; ++k)
            {
                dot += r[3 * i + k] * r[3 * j + k];
            }
            if (fabs(dot - (i == j ? 1.0 : 0.0)) > tolerance)
            {
                *message = "The rotation matrix must be orthonormal";
                return false;
            }
        }
    }
    const double det = r[0] * (r[4] * r[8] - r[5] * r[7])
                       - r[1] * (r[3] * r[8] - r[5] * r[6])
                       + r[2] * (r[3] * r[7] - r[4] * r[6]);
    if (fabs(det - 1.0) > tolerance)
    {
        *message = "The rotation matrix must not mirror (determinant 1)";
        return false;
    }
    return true;
}

bool x3pio_transformation_valid(const x3pio_transformation *transformation,
                                const char **message)
{
    for (size_t i = 0; i < 3; ++i)
    {
        if (!isfinite(transformation->offset[i]))
        {
            *message = "The offset of an axis must be finite";
            return false;
        }
    }
    return rotation_valid(transformation, message);
}

/* The increment of an axis: positive and finite; and for the z axis of integers
 * it can be left to the writer. */
static bool increment_valid(const x3pio_axis *axis, bool may_be_auto)
{
    if (isfinite(axis->increment) && axis->increment > 0.0)
    {
        return true;
    }
    return may_be_auto && axis->increment == X3PIO_AUTO_INCREMENT
           && (axis->storage == X3PIO_TYPE_INT16
               || axis->storage == X3PIO_TYPE_INT32);
}

bool x3pio_axis_valid(const x3pio_axis *axis, bool may_be_auto)
{
    return (axis->type == X3PIO_AXIS_INCREMENTAL
            || axis->type == X3PIO_AXIS_ABSOLUTE)
           && increment_valid(axis, may_be_auto)
           && (axis->type == X3PIO_AXIS_INCREMENTAL
               || x3pio_type_is_stored(axis->storage));
}

x3pio_status x3pio_document_validate(const x3pio_document *d,
                                     const char **message)
{
    const char *text = "";
    if (message != NULL)
    {
        *message = text;
    }
    if (d == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
#define FAIL(reason)                                                           \
    do                                                                         \
    {                                                                          \
        text = (reason);                                                       \
        if (message != NULL)                                                   \
        {                                                                      \
            *message = text;                                                   \
        }                                                                      \
        return X3PIO_ERR_ARGUMENT;                                             \
    } while (0)
    if (d->axes[2].type != X3PIO_AXIS_ABSOLUTE)
    {
        FAIL("The z axis must be absolute");
    }
    for (size_t k = 0; k < 3; ++k)
    {
        if (!x3pio_axis_valid(&d->axes[k], k == 2))
        {
            FAIL("The increment of an axis must be finite and positive, and "
                 "an absolute axis needs a type that files store");
        }
    }
    if (!x3pio_transformation_valid(&d->transformation, &text))
    {
        if (message != NULL)
        {
            *message = text;
        }
        return X3PIO_ERR_ARGUMENT;
    }
    const bool cloud = d->feature == X3PIO_POINT_CLOUD;
    if (cloud != d->dims.list)
    {
        FAIL("A point cloud is stored as a list, a profile or surface as a "
             "matrix");
    }
    if (cloud
        && (d->axes[0].type != X3PIO_AXIS_ABSOLUTE
            || d->axes[1].type != X3PIO_AXIS_ABSOLUTE))
    {
        FAIL("All axes of a point cloud must be absolute");
    }
    if (d->feature == X3PIO_PROFILE && d->dims.size_y != 1)
    {
        FAIL("A profile has a matrix with one row (size_y of 1)");
    }
    size_t count = 0;
    if (!x3pio_multiply(d->dims.size_x, d->dims.size_y, &count)
        || !x3pio_multiply(count, d->dims.size_z, &count))
    {
        FAIL("The dimensions of the points are too large");
    }
    if (count == 0)
    {
        FAIL("The dimensions of the point data must not be zero");
    }
    if (d->has_metadata)
    {
        if (!d->has_date)
        {
            FAIL("The metadata needs a date");
        }
        if (d->probing == X3PIO_PROBING_UNKNOWN)
        {
            FAIL("The metadata needs the type of the probing system");
        }
    }
#undef FAIL
    return X3PIO_OK;
}
