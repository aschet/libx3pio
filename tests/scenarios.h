/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The shapes of files that the tests write and read back: what the points are,
 * how they are described, and what a reader must give back. */

#ifndef X3PIO_TESTS_SCENARIOS_H
#define X3PIO_TESTS_SCENARIOS_H

#include "check.h"

#include <math.h>

typedef struct scenario
{
    const char *name;
    x3pio_feature feature;
    size_t columns;
    size_t rows;
    size_t layers;
    /* x and y are incremental: the points lie on a grid. */
    bool regular;
    /* The type of the stored axes. */
    x3pio_type type;
    /* The increment of the stored axes. */
    double increment;
    /* The view lies in the global system by a rotation and an offset. */
    bool placed;
    /* Some points are not measured. */
    bool missing;
} scenario;

static const scenario scenarios[] = {
    {"surface double", X3PIO_SURFACE, 20, 15, 1, true, X3PIO_TYPE_DOUBLE, 1.0,
     false, false},
    {"surface float with missing points", X3PIO_SURFACE, 20, 15, 1, true,
     X3PIO_TYPE_FLOAT, 1.0, false, true},
    {"surface int16 with missing points", X3PIO_SURFACE, 20, 15, 1, true,
     X3PIO_TYPE_INT16, 2e-10, false, true},
    {"surface int32 with missing points", X3PIO_SURFACE, 20, 15, 1, true,
     X3PIO_TYPE_INT32, 1e-12, false, true},
    {"profile double", X3PIO_PROFILE, 64, 1, 1, true, X3PIO_TYPE_DOUBLE, 1.0,
     false, false},
    {"profile in layers", X3PIO_PROFILE, 32, 1, 3, true, X3PIO_TYPE_FLOAT, 1.0,
     false, true},
    {"surface in layers int16", X3PIO_SURFACE, 8, 6, 3, true, X3PIO_TYPE_INT16,
     2e-10, false, true},
    {"irregular surface", X3PIO_SURFACE, 10, 8, 1, false, X3PIO_TYPE_DOUBLE,
     1.0, false, false},
    {"irregular layers int32", X3PIO_SURFACE, 6, 5, 2, false, X3PIO_TYPE_INT32,
     1e-9, false, true},
    {"placed regular surface", X3PIO_SURFACE, 12, 9, 1, true, X3PIO_TYPE_DOUBLE,
     1.0, true, false},
    {"placed irregular surface float", X3PIO_SURFACE, 7, 6, 1, false,
     X3PIO_TYPE_FLOAT, 1.0, true, true},
    {"point cloud float", X3PIO_POINT_CLOUD, 100, 1, 1, false, X3PIO_TYPE_FLOAT,
     1.0, false, false},
    {"point cloud int16", X3PIO_POINT_CLOUD, 100, 1, 1, false, X3PIO_TYPE_INT16,
     2e-6, false, false},
    {"point cloud int32", X3PIO_POINT_CLOUD, 100, 1, 1, false, X3PIO_TYPE_INT32,
     1e-9, true, false},
    {"point cloud double", X3PIO_POINT_CLOUD, 100, 1, 1, false,
     X3PIO_TYPE_DOUBLE, 1.0, false, false},
};
#define SCENARIO_COUNT (sizeof scenarios / sizeof scenarios[0])

#define GRID_STEP 1e-6

static inline size_t scenario_points(const scenario *s)
{
    return s->columns * s->rows * s->layers;
}

/* The rows of the matrix that the functions transfer: the rows of the layers
 * one after the other, and a row for each point of a list. */
static inline size_t scenario_rows(const scenario *s)
{
    return s->feature == X3PIO_POINT_CLOUD ? s->columns : s->rows * s->layers;
}

static inline bool scenario_is_missing(const scenario *s, size_t i)
{
    return s->missing && i % 17 == 5;
}

/* The rotation of 10 degrees about x, and an offset. */
static inline x3pio_placement scenario_placement(const scenario *s)
{
    x3pio_placement place = x3pio_placement_identity();
    if (s->placed)
    {
        const double c = cos(10.0 * 3.14159265358979323846 / 180.0);
        const double sn = sin(10.0 * 3.14159265358979323846 / 180.0);
        const double rotation[9] = {1, 0, 0, 0, c, -sn, 0, sn, c};
        memcpy(place.rotation, rotation, sizeof rotation);
        place.offset[0] = 1e-3;
        place.offset[1] = 2e-3;
        place.offset[2] = 3e-3;
    }
    return place;
}

/* The coordinates of the points in the view system, as a program holds them
 * before they are written: before they are rounded to the stored type. */
static inline void scenario_points_view(const scenario *s, double *x, double *y,
                                        double *z)
{
    const size_t count = scenario_points(s);
    for (size_t i = 0; i < count; ++i)
    {
        const size_t u = i % s->columns;
        const size_t v = (i / s->columns) % s->rows;
        const size_t w = i / (s->columns * s->rows);
        const double wobble = 1e-8 * sin((double)i);
        if (s->feature == X3PIO_POINT_CLOUD)
        {
            x[i] = 0.01 + 0.02 * sin(0.3 * (double)i);
            y[i] = 0.02 + 0.015 * cos(0.2 * (double)i);
            z[i] = 0.005 * sin(0.1 * (double)i);
        }
        else
        {
            x[i] = s->regular ? (double)u * GRID_STEP
                              : (double)u * GRID_STEP + wobble;
            y[i] = s->regular ? (double)v * GRID_STEP
                              : (double)v * GRID_STEP - wobble;
            z[i] = 4e-6 * sin(0.2 * (double)u) * cos(0.3 * (double)v)
                   + 5e-7 * (double)w;
        }
        if (scenario_is_missing(s, i))
        {
            z[i] = NAN;
        }
    }
}

/* What the stored type holds of a value. */
static inline double scenario_stored(x3pio_type type, double increment,
                                     double v)
{
    if (isnan(v))
    {
        return v;
    }
    switch (type)
    {
    case X3PIO_TYPE_INT16:
    case X3PIO_TYPE_INT32:
        return nearbyint(v / increment) * increment;
    case X3PIO_TYPE_FLOAT:
        return (double)(float)(v / increment) * increment;
    default:
        return v;
    }
}

/* The tolerance for a stored value: the step of the type. */
static inline double scenario_tolerance(const scenario *s)
{
    return s->type == X3PIO_TYPE_DOUBLE
               ? 1e-18
               : (s->type == X3PIO_TYPE_FLOAT ? 1e-12 : s->increment * 1e-3);
}

/* The document of a scenario. */
static inline x3pio_document *scenario_document(const scenario *s,
                                                x3pio_revision revision)
{
    x3pio_document *d = NULL;
    CHECK_OK(x3pio_document_create(&d));
    CHECK_OK(x3pio_document_set_revision(d, revision));
    if (s->feature == X3PIO_POINT_CLOUD)
    {
        CHECK_OK(x3pio_document_set_point_cloud(d, s->columns, s->type,
                                                s->increment));
    }
    else if (!s->regular)
    {
        CHECK_OK(x3pio_document_set_irregular_surface(
            d, s->columns, s->rows, s->type, s->increment, s->layers));
    }
    else if (s->feature == X3PIO_PROFILE)
    {
        CHECK_OK(x3pio_document_set_profile(d, s->columns, GRID_STEP, s->type,
                                            s->increment, s->layers));
    }
    else
    {
        CHECK_OK(x3pio_document_set_surface(d, s->columns, s->rows, GRID_STEP,
                                            GRID_STEP, s->type, s->increment,
                                            s->layers));
    }
    const x3pio_placement place = scenario_placement(s);
    CHECK_OK(x3pio_document_set_placement(d, &place));
    x3pio_timestamp date = {2026, 3, 4, 12, 30, 0, 0, true, 60};
    CHECK_OK(x3pio_document_set_date(d, &date));
    CHECK_OK(x3pio_document_set_probing_type(d, X3PIO_PROBING_SOFTWARE));
    CHECK_OK(x3pio_document_set_model(d, s->name));
    return d;
}

#endif /* X3PIO_TESTS_SCENARIOS_H */
