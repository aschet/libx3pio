/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Points in the global coordinate system: the transformation of the document is
 * applied while the points are read, and undone while they are written. */

#include "helpers.h"

#include <stdint.h>

enum
{
    COLUMNS = 4,
    ROWS = 3,
    COUNT = COLUMNS * ROWS
};

/* A turn by 90 degrees about z, then a large shift: the machine coordinates of
 * a part that sits far from the origin. */
static x3pio_transformation transformation(void)
{
    x3pio_transformation p = x3pio_transformation_identity();
    p.rotation[0] = 0.0;
    p.rotation[1] = -1.0;
    p.rotation[3] = 1.0;
    p.rotation[4] = 0.0;
    p.offset[0] = 1000.0;
    p.offset[1] = -2000.0;
    p.offset[2] = 3.0;
    return p;
}

static x3pio_reader *open_bytes(const x3pio_buffer *bytes)
{
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes->data, bytes->size));
    return reader;
}

static const x3pio_layout *packed(x3pio_type type)
{
    return x3pio_layout_packed(type);
}

static x3pio_fields fields_of(void *x, void *y, void *z)
{
    x3pio_fields f;
    x3pio_fields_init(&f);
    f.x = (x3pio_field){x, *packed(X3PIO_TYPE_DOUBLE)};
    f.y = (x3pio_field){y, *packed(X3PIO_TYPE_DOUBLE)};
    f.z = (x3pio_field){z, *packed(X3PIO_TYPE_DOUBLE)};
    return f;
}

static x3pio_const_fields const_fields_of(const double *x, const double *y,
                                          const double *z)
{
    x3pio_const_fields f;
    x3pio_const_fields_init(&f);
    f.x = (x3pio_const_field){x, *packed(X3PIO_TYPE_DOUBLE)};
    f.y = (x3pio_const_field){y, *packed(X3PIO_TYPE_DOUBLE)};
    f.z = (x3pio_const_field){z, *packed(X3PIO_TYPE_DOUBLE)};
    return f;
}

/* A cloud written in global coordinates reads back in the view system as the
 * inverse of the transformation, and in the global one as it was. */
static void test_cloud(x3pio_type type, double increment, double tolerance)
{
    const x3pio_transformation p = transformation();
    double view[COUNT][3];
    double gx[COUNT];
    double gy[COUNT];
    double gz[COUNT];
    for (size_t i = 0; i < COUNT; ++i)
    {
        view[i][0] = 1e-3 * (double)i;
        view[i][1] = 2e-3 * (double)(i % 5);
        view[i][2] = 1e-4 * (double)(i * i);
        double global[3];
        x3pio_transformation_to_global(&p, view[i], global);
        gx[i] = global[0];
        gy[i] = global[1];
        gz[i] = global[2];
    }
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(&doc));
    CHECK_OK(
        x3pio_document_set_feature_point_cloud(doc, COUNT, type, increment));
    CHECK_OK(x3pio_document_set_transformation(doc, &p));
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    /* In two parts. */
    x3pio_const_fields from = const_fields_of(gx, gy, gz);
    CHECK_OK(x3pio_write_points_in(writer, X3PIO_COORDINATES_GLOBAL, &from, 5));
    from = const_fields_of(gx + 5, gy + 5, gz + 5);
    CHECK_OK(x3pio_write_points_in(writer, X3PIO_COORDINATES_GLOBAL, &from,
                                   COUNT - 5));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);

    double x[COUNT];
    double y[COUNT];
    double z[COUNT];
    x3pio_fields into = fields_of(x, y, z);
    x3pio_reader *reader = open_bytes(&bytes);
    CHECK_OK(x3pio_read_points(reader, &into, COUNT));
    for (size_t i = 0; i < COUNT; ++i)
    {
        CHECK_NEAR(x[i], view[i][0], tolerance);
        CHECK_NEAR(y[i], view[i][1], tolerance);
        CHECK_NEAR(z[i], view[i][2], tolerance);
    }
    x3pio_reader_free(reader);

    reader = open_bytes(&bytes);
    CHECK_OK(x3pio_read_points_in(reader, X3PIO_COORDINATES_GLOBAL, &into, 7));
    into = fields_of(x + 7, y + 7, z + 7);
    CHECK_OK(x3pio_read_points_in(reader, X3PIO_COORDINATES_GLOBAL, &into,
                                  COUNT - 7));
    for (size_t i = 0; i < COUNT; ++i)
    {
        CHECK_NEAR(x[i], gx[i], tolerance);
        CHECK_NEAR(y[i], gy[i], tolerance);
        CHECK_NEAR(z[i], gz[i], tolerance);
    }
    x3pio_reader_free(reader);
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);
}

/* The rotation happens in double precision, before a coordinate is narrowed to
 * the float of the program. */
static void test_float_buffers(void)
{
    const x3pio_transformation p = transformation();
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(&doc));
    CHECK_OK(
        x3pio_document_set_feature_point_cloud(doc, 2, X3PIO_TYPE_DOUBLE, 1.0));
    CHECK_OK(x3pio_document_set_transformation(doc, &p));
    const double vx[2] = {1.234567e-3, 2.5e-3};
    const double vy[2] = {-7.654321e-3, 1e-3};
    const double vz[2] = {1.5e-6, 2.5e-6};
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    const x3pio_const_fields from = const_fields_of(vx, vy, vz);
    CHECK_OK(x3pio_write_points(writer, &from, 2));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);

    float x[2];
    float y[2];
    float z[2];
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.x = (x3pio_field){x, *packed(X3PIO_TYPE_FLOAT)};
    into.y = (x3pio_field){y, *packed(X3PIO_TYPE_FLOAT)};
    into.z = (x3pio_field){z, *packed(X3PIO_TYPE_FLOAT)};
    x3pio_reader *reader = open_bytes(&bytes);
    CHECK_OK(x3pio_read_points_in(reader, X3PIO_COORDINATES_GLOBAL, &into, 2));
    for (size_t i = 0; i < 2; ++i)
    {
        const double view[3] = {vx[i], vy[i], vz[i]};
        double global[3];
        x3pio_transformation_to_global(&p, view, global);
        CHECK(x[i] == (float)global[0]);
        CHECK(y[i] == (float)global[1]);
        CHECK(z[i] == (float)global[2]);
    }
    x3pio_reader_free(reader);
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);
}

/* A regular grid stores no x and y, and a point that was not measured has no
 * place in any of the three coordinates. */
static void test_grid(void)
{
    const x3pio_transformation p = transformation();
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(&doc));
    CHECK_OK(x3pio_document_set_feature_surface(doc, COLUMNS, ROWS, 1e-3, 2e-3,
                                                X3PIO_TYPE_FLOAT, 1.0, 1));
    CHECK_OK(x3pio_document_set_transformation(doc, &p));
    double gx[COUNT];
    double gy[COUNT];
    double gz[COUNT];
    double view_z[COUNT];
    for (size_t i = 0; i < COUNT; ++i)
    {
        const size_t column = i % COLUMNS;
        const size_t row = i / COLUMNS;
        const double view[3] = {1e-3 * (double)column, 2e-3 * (double)row,
                                (i == 5 ? NAN : 1e-6 * (double)i)};
        view_z[i] = view[2];
        double global[3];
        x3pio_transformation_to_global(&p, view, global);
        gx[i] = global[0];
        gy[i] = global[1];
        gz[i] = global[2];
    }
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    const x3pio_const_fields from = const_fields_of(gx, gy, gz);
    CHECK_OK(
        x3pio_write_points_in(writer, X3PIO_COORDINATES_GLOBAL, &from, ROWS));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);

    /* The heights of the file are the z of the view system. */
    x3pio_reader *reader = open_bytes(&bytes);
    double back[COUNT];
    CHECK_OK(x3pio_read_heights(reader, packed(X3PIO_TYPE_DOUBLE), back));
    for (size_t i = 0; i < COUNT; ++i)
    {
        if (i == 5)
        {
            CHECK(isnan(back[i]));
        }
        else
        {
            CHECK_NEAR(back[i], view_z[i], 1e-9);
        }
    }
    x3pio_reader_free(reader);

    double x[COUNT];
    double y[COUNT];
    double z[COUNT];
    x3pio_fields into = fields_of(x, y, z);
    reader = open_bytes(&bytes);
    CHECK_OK(
        x3pio_read_points_in(reader, X3PIO_COORDINATES_GLOBAL, &into, ROWS));
    for (size_t i = 0; i < COUNT; ++i)
    {
        if (i == 5)
        {
            CHECK(isnan(x[i]) && isnan(y[i]) && isnan(z[i]));
        }
        else
        {
            CHECK_NEAR(x[i], gx[i], 1e-6);
            CHECK_NEAR(y[i], gy[i], 1e-6);
            CHECK_NEAR(z[i], gz[i], 1e-6);
        }
    }
    x3pio_reader_free(reader);
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);
}

static void test_arguments(void)
{
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(&doc));
    CHECK_OK(
        x3pio_document_set_feature_point_cloud(doc, 2, X3PIO_TYPE_DOUBLE, 1.0));
    double a[2] = {0, 0};
    double b[2] = {0, 0};
    double c[2] = {0, 0};
    uint8_t mask[2];

    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_write_points_in(NULL, X3PIO_COORDINATES_GLOBAL, NULL, 2));
    x3pio_const_fields from = const_fields_of(a, b, NULL);
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_write_points_in(writer, X3PIO_COORDINATES_GLOBAL, &from, 2));
    from = const_fields_of(a, b, c);
    from.valid = (x3pio_const_field){mask, *packed(X3PIO_TYPE_UINT8)};
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_write_points_in(writer, X3PIO_COORDINATES_GLOBAL, &from, 2));
    from.valid.data = NULL;
    CHECK_OK(x3pio_write_points_in(writer, X3PIO_COORDINATES_GLOBAL, &from, 2));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);

    x3pio_reader *reader = open_bytes(&bytes);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_read_points_in(NULL, X3PIO_COORDINATES_GLOBAL, NULL, 2));
    x3pio_fields into = fields_of(a, b, NULL);
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_read_points_in(reader, X3PIO_COORDINATES_GLOBAL, &into, 2));
    into = fields_of(a, b, c);
    into.valid = (x3pio_field){mask, *packed(X3PIO_TYPE_UINT8)};
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_read_points_in(reader, X3PIO_COORDINATES_GLOBAL, &into, 2));
    into.valid.data = NULL;
    /* The refusals do not use up rows. */
    CHECK_OK(x3pio_read_points_in(reader, X3PIO_COORDINATES_GLOBAL, &into, 2));
    x3pio_reader_free(reader);
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);
}

/* The offset without the rotation: what libraries that ignore the rotation
 * give. Each axis is shifted alone, so z can be bound without x and y. */
static void test_translated(void)
{
    const x3pio_transformation p = transformation();
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(&doc));
    CHECK_OK(x3pio_document_set_feature_surface(doc, COLUMNS, ROWS, 1e-3, 2e-3,
                                                X3PIO_TYPE_DOUBLE, 1.0, 1));
    CHECK_OK(x3pio_document_set_transformation(doc, &p));
    double z[COUNT];
    for (size_t i = 0; i < COUNT; ++i)
    {
        z[i] = i == 7 ? NAN : 1e-6 * (double)i + 3.0;
    }
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    x3pio_const_fields from = const_fields_of(NULL, NULL, z);
    CHECK_OK(x3pio_write_points_in(writer, X3PIO_COORDINATES_VIEW_OFFSET, &from,
                                   ROWS));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);

    double view_z[COUNT];
    double translated_z[COUNT];
    double x[COUNT];
    double y[COUNT];
    x3pio_fields into = fields_of(NULL, NULL, view_z);
    x3pio_reader *reader = open_bytes(&bytes);
    CHECK_OK(x3pio_read_points(reader, &into, ROWS));
    x3pio_reader_free(reader);
    into = fields_of(x, y, translated_z);
    reader = open_bytes(&bytes);
    CHECK_OK(x3pio_read_points_in(reader, X3PIO_COORDINATES_VIEW_OFFSET, &into,
                                  ROWS));
    x3pio_reader_free(reader);
    for (size_t i = 0; i < COUNT; ++i)
    {
        if (i == 7)
        {
            CHECK(isnan(view_z[i]) && isnan(translated_z[i]));
            continue;
        }
        CHECK_NEAR(view_z[i], 1e-6 * (double)i, 1e-12);
        CHECK_NEAR(translated_z[i], z[i], 1e-12);
        /* Only the offset is added: the grid is not turned. */
        const size_t column = i % COLUMNS;
        const size_t row = i / COLUMNS;
        CHECK_NEAR(x[i], 1e-3 * (double)column + p.offset[0], 1e-9);
        CHECK_NEAR(y[i], 2e-3 * (double)row + p.offset[1], 1e-9);
    }
    /* A coordinate system that does not exist. */
    reader = open_bytes(&bytes);
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_read_points_in(reader, (x3pio_coordinate_system)7, &into, ROWS));
    x3pio_reader_free(reader);
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_read_points_in(NULL, (x3pio_coordinate_system)7, &into, ROWS));
    x3pio_reader_free(NULL);
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);
}

/* The layers of a file are rows that follow each other: each call can read one
 * into an array of its own. */
static void test_layers_in_separate_buffers(void)
{
    enum
    {
        LAYERS = 3,
        PIXELS = COLUMNS * ROWS
    };
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(&doc));
    CHECK_OK(x3pio_document_set_feature_surface(
        doc, COLUMNS, ROWS, 1e-3, 1e-3, X3PIO_TYPE_DOUBLE, 1.0, LAYERS));
    double images[LAYERS][PIXELS];
    for (size_t l = 0; l < LAYERS; ++l)
    {
        for (size_t i = 0; i < PIXELS; ++i)
        {
            images[l][i] = 1e-6 * (double)(100 * l + i);
        }
    }
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    for (size_t l = 0; l < LAYERS; ++l)
    {
        const x3pio_const_fields from = const_fields_of(NULL, NULL, images[l]);
        CHECK_OK(x3pio_write_points(writer, &from, ROWS));
    }
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);

    double back[LAYERS][PIXELS];
    x3pio_reader *reader = open_bytes(&bytes);
    for (size_t l = 0; l < LAYERS; ++l)
    {
        const x3pio_fields into = fields_of(NULL, NULL, back[l]);
        CHECK_OK(x3pio_read_points(reader, &into, ROWS));
    }
    for (size_t l = 0; l < LAYERS; ++l)
    {
        for (size_t i = 0; i < PIXELS; ++i)
        {
            CHECK_NEAR(back[l][i], images[l][i], 1e-15);
        }
    }
    /* All rows are used up. */
    const x3pio_fields more = fields_of(NULL, NULL, back[0]);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, &more, 1));
    x3pio_reader_free(reader);
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);
}

int main(void)
{
    test_cloud(X3PIO_TYPE_DOUBLE, 1.0, 1e-9);
    test_cloud(X3PIO_TYPE_INT32, 1e-9, 2e-9);
    test_float_buffers();
    test_grid();
    test_arguments();
    test_translated();
    test_layers_in_separate_buffers();
    return finish_tests();
}
