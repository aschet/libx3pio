/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The feature type, the size and the axes of a document, set together.
 */

#include "document.h"
#include <math.h>

/* ---- feature type, size and axes ---------------------------------------- */

static bool step_valid(double step)
{
    return isfinite(step) && step > 0.0;
}

x3pio_status x3pio_document_set_feature_surface(
    x3pio_document *document, size_t columns, size_t rows, double x_step,
    double y_step, x3pio_type z_type, double z_increment, size_t layers)
{
    if (document == NULL || !step_valid(x_step) || !step_valid(y_step)
        || !x3pio_type_is_stored(z_type)
        || !(
            step_valid(z_increment)
            || (z_increment == X3PIO_AUTO_INCREMENT
                && (z_type == X3PIO_TYPE_INT16 || z_type == X3PIO_TYPE_INT32))))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    document->feature = X3PIO_SURFACE;
    document->axes[0] =
        (x3pio_axis){X3PIO_AXIS_INCREMENTAL, X3PIO_TYPE_DOUBLE, x_step};
    document->axes[1] =
        (x3pio_axis){X3PIO_AXIS_INCREMENTAL, X3PIO_TYPE_DOUBLE, y_step};
    document->axes[2] = (x3pio_axis){X3PIO_AXIS_ABSOLUTE, z_type, z_increment};
    document->dims = (x3pio_dimensions){columns, rows, layers, false};
    return X3PIO_OK;
}

x3pio_status x3pio_document_set_feature_profile(x3pio_document *document,
                                                size_t points, double x_step,
                                                x3pio_type z_type,
                                                double z_increment,
                                                size_t layers)
{
    const x3pio_status status = x3pio_document_set_feature_surface(
        document, points, 1, x_step, x_step, z_type, z_increment, layers);
    if (status == X3PIO_OK)
    {
        document->feature = X3PIO_PROFILE;
    }
    return status;
}

x3pio_status x3pio_document_set_feature_point_cloud(x3pio_document *document,
                                                    size_t count,
                                                    x3pio_type type,
                                                    double increment)
{
    if (document == NULL || !x3pio_type_is_stored(type)
        || !step_valid(increment))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    document->feature = X3PIO_POINT_CLOUD;
    for (size_t k = 0; k < 3; ++k)
    {
        document->axes[k] = (x3pio_axis){X3PIO_AXIS_ABSOLUTE, type, increment};
    }
    document->dims = (x3pio_dimensions){count, 1, 1, true};
    return X3PIO_OK;
}

x3pio_status x3pio_document_set_feature_irregular_surface(
    x3pio_document *document, size_t columns, size_t rows, x3pio_type type,
    double increment, size_t layers)
{
    const x3pio_status status =
        x3pio_document_set_feature_point_cloud(document, 0, type, increment);
    if (status == X3PIO_OK)
    {
        document->feature = X3PIO_SURFACE;
        document->dims = (x3pio_dimensions){columns, rows, layers, false};
    }
    return status;
}

x3pio_status
x3pio_document_set_feature_irregular_profile(x3pio_document *document,
                                             size_t points, x3pio_type type,
                                             double increment, size_t layers)
{
    if (!step_valid(increment))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    const x3pio_status status = x3pio_document_set_feature_profile(
        document, points, 1.0, type, increment, layers);
    if (status == X3PIO_OK)
    {
        document->axes[X3PIO_X] =
            (x3pio_axis){X3PIO_AXIS_ABSOLUTE, type, increment};
    }
    return status;
}
