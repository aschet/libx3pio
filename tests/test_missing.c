/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Points that were not measured: NaN for floats, the mask of bits for integers,
 * and integer data that never becomes a float. */

#include "helpers.h"

#include <stdint.h>

#define COLUMNS 6
#define ROWS 4
#define COUNT (COLUMNS * ROWS)

static x3pio_document *grid_document(x3pio_type type, double increment)
{
    x3pio_document *d = NULL;
    CHECK_OK(x3pio_document_create(&d));
    CHECK_OK(x3pio_document_set_feature_surface(d, COLUMNS, ROWS, 1e-6, 1e-6,
                                                type, increment, 1));
    return d;
}

static bool is_hole(size_t i)
{
    return i == 3 || i == 4 || i == 13 || i == 23;
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

/* A file of floats marks a point by NaN; the mask of the program marks the
 * same points, and the file is the same. */
static void test_floats(void)
{
    for (int st = 0; st < 2; ++st)
    {
        const x3pio_storage storage =
            st == 0 ? X3PIO_STORAGE_BINARY : X3PIO_STORAGE_TEXT;
        x3pio_document *doc = grid_document(X3PIO_TYPE_FLOAT, 1.0);
        float z[COUNT];
        float with_nan[COUNT];
        unsigned char mask[COUNT];
        for (size_t i = 0; i < COUNT; ++i)
        {
            z[i] = 0.001F * (float)(i + 1);
            with_nan[i] = is_hole(i) ? NAN : z[i];
            mask[i] = is_hole(i) ? 0 : 1;
        }
        x3pio_buffer by_mask;
        x3pio_buffer by_nan;
        x3pio_buffer_init(&by_mask, x3pio_default_allocator());
        x3pio_buffer_init(&by_nan, x3pio_default_allocator());

        x3pio_writer *writer = NULL;
        CHECK_OK(x3pio_writer_create(&writer));
        CHECK_OK(x3pio_writer_set_storage(writer, storage));
        CHECK_OK(create_in_buffer(writer, doc, &by_mask));
        x3pio_const_fields from;
        x3pio_const_fields_init(&from);
        from.z = (x3pio_const_field){z, *packed(X3PIO_TYPE_FLOAT)};
        from.valid = (x3pio_const_field){mask, *packed(X3PIO_TYPE_UINT8)};
        CHECK_OK(x3pio_write_data(writer, &from, COUNT));
        CHECK_OK(x3pio_end_file(writer));
        x3pio_writer_free(writer);

        CHECK_OK(x3pio_writer_create(&writer));
        CHECK_OK(x3pio_writer_set_storage(writer, storage));
        CHECK_OK(create_in_buffer(writer, doc, &by_nan));
        x3pio_const_fields_init(&from);
        from.z = (x3pio_const_field){with_nan, *packed(X3PIO_TYPE_FLOAT)};
        CHECK_OK(x3pio_write_data(writer, &from, COUNT));
        CHECK_OK(x3pio_end_file(writer));
        x3pio_writer_free(writer);
        CHECK(by_mask.size == by_nan.size
              && memcmp(by_mask.data, by_nan.data, by_nan.size) == 0);

        /* In metres a point that was not measured is NaN. */
        x3pio_reader *reader = open_bytes(&by_nan);
        CHECK(x3pio_document_has_validity(x3pio_reader_document(reader))
              == (storage == X3PIO_STORAGE_TEXT));
        double metres[COUNT];
        CHECK_OK(x3pio_read_z(reader, packed(X3PIO_TYPE_DOUBLE), metres));
        for (size_t i = 0; i < COUNT; ++i)
        {
            CHECK(isnan(metres[i]) == is_hole(i));
            if (!is_hole(i))
            {
                CHECK_NEAR(metres[i], (double)z[i], 1e-9);
            }
        }
        x3pio_reader_free(reader);

        /* As stored, with the mask: the numbers of the file, NaN where a float
         * file has no value. */
        reader = open_bytes(&by_nan);
        float stored[COUNT];
        unsigned char flags[COUNT];
        x3pio_fields into;
        x3pio_fields_init(&into);
        into.z = (x3pio_field){stored, *packed(X3PIO_TYPE_FLOAT)};
        into.valid = (x3pio_field){flags, *packed(X3PIO_TYPE_UINT8)};
        CHECK_OK(x3pio_read_data(reader, &into, COUNT));
        for (size_t i = 0; i < COUNT; ++i)
        {
            CHECK(flags[i] == (is_hole(i) ? 0 : 1));
            CHECK(isnan(stored[i]) == is_hole(i));
            if (!is_hole(i))
            {
                CHECK(stored[i] == z[i]);
            }
        }
        x3pio_reader_free(reader);
        x3pio_buffer_free(&by_mask);
        x3pio_buffer_free(&by_nan);
        x3pio_document_free(doc);
    }
}

/* The standard defines no value for a point of integers that was not measured,
 * and the mask says which they are. The writer stores 0 for a NaN in metres, so
 * that a file does not depend on what the program had in the buffer. */
static void test_integer_value_of_missing_points(void)
{
    x3pio_document *doc = grid_document(X3PIO_TYPE_INT16, 1e-9);
    double heights[COUNT];
    for (size_t i = 0; i < COUNT; ++i)
    {
        heights[i] = is_hole(i) ? NAN : 1e-9 * (double)((int)i * 100 - 500);
    }
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    CHECK_OK(x3pio_write_z(writer, packed(X3PIO_TYPE_DOUBLE), heights));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);

    archive a = {0};
    CHECK(archive_unzip(&a, bytes.data, bytes.size));
    member *data = archive_find(&a, "bindata/data.bin");
    CHECK(data != NULL && data->data.size == COUNT * 2);
    if (data != NULL)
    {
        for (size_t i = 0; i < COUNT; ++i)
        {
            const int16_t got = (int16_t)(data->data.data[2 * i]
                                          | (data->data.data[2 * i + 1] << 8));
            CHECK(got == (is_hole(i) ? 0 : (int)i * 100 - 500));
        }
    }
    CHECK(archive_find(&a, "bindata/valid.bin") != NULL);
    archive_free(&a);
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);
}

/* Integers have no NaN: they carry the points that were not measured in a file
 * of bits. */
static void test_integer_mask(void)
{
    x3pio_document *doc = grid_document(X3PIO_TYPE_INT16, 1e-9);
    int16_t counts[COUNT];
    unsigned char mask[COUNT];
    for (size_t i = 0; i < COUNT; ++i)
    {
        counts[i] = (int16_t)((int)i * 100 - 500);
        mask[i] = is_hole(i) ? 0 : 1;
    }
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    x3pio_const_fields from;
    x3pio_const_fields_init(&from);
    from.z = (x3pio_const_field){counts, *packed(X3PIO_TYPE_INT16)};
    from.valid = (x3pio_const_field){mask, *packed(X3PIO_TYPE_UINT8)};
    CHECK_OK(x3pio_write_data(writer, &from, COUNT));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);

    archive a = {0};
    CHECK(archive_unzip(&a, bytes.data, bytes.size));
    member *valid = archive_find(&a, "bindata/valid.bin");
    CHECK(valid != NULL && valid->data.size == (COUNT + 7) / 8);
    if (valid != NULL)
    {
        /* Bit j is bit j % 8 of byte j / 8, least significant first, 1 for a
         * point that was measured; the bits after the last point are 0. */
        for (size_t i = 0; i < COUNT; ++i)
        {
            const unsigned bit = (valid->data.data[i / 8] >> (i % 8)) & 1U;
            CHECK(bit == (is_hole(i) ? 0U : 1U));
        }
        unsigned char packed_bits[(COUNT + 7) / 8];
        x3pio_validity_pack(mask, COUNT, packed_bits);
        CHECK(memcmp(packed_bits, valid->data.data, sizeof packed_bits) == 0);
    }
    /* The values of the points that were not measured are the ones given. */
    member *data = archive_find(&a, "bindata/data.bin");
    CHECK(data != NULL && data->data.size == COUNT * 2);
    if (data != NULL)
    {
        for (size_t i = 0; i < COUNT; ++i)
        {
            const int16_t got = (int16_t)(data->data.data[2 * i]
                                          | (data->data.data[2 * i + 1] << 8));
            CHECK(got == counts[i]);
        }
    }
    archive_free(&a);

    x3pio_reader *reader = open_bytes(&bytes);
    CHECK(x3pio_document_has_validity(x3pio_reader_document(reader)));
    int16_t back[COUNT];
    unsigned char flags[COUNT];
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.z = (x3pio_field){back, *packed(X3PIO_TYPE_INT16)};
    into.valid = (x3pio_field){flags, *packed(X3PIO_TYPE_UINT8)};
    CHECK_OK(x3pio_read_data(reader, &into, COUNT));
    CHECK(memcmp(back, counts, sizeof counts) == 0);
    CHECK(memcmp(flags, mask, sizeof mask) == 0);
    x3pio_reader_free(reader);

    /* In metres the points that were not measured are NaN. */
    reader = open_bytes(&bytes);
    float heights[COUNT];
    CHECK_OK(x3pio_read_z(reader, packed(X3PIO_TYPE_FLOAT), heights));
    for (size_t i = 0; i < COUNT; ++i)
    {
        CHECK(isnan(heights[i]) == is_hole(i));
        if (!is_hole(i))
        {
            CHECK_NEAR(heights[i], (double)counts[i] * 1e-9, 1e-12);
        }
    }
    x3pio_reader_free(reader);

    /* Without a mask all points count as measured. */
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    x3pio_const_fields_init(&from);
    from.z = (x3pio_const_field){counts, *packed(X3PIO_TYPE_INT16)};
    CHECK_OK(x3pio_write_data(writer, &from, COUNT));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);
    reader = open_bytes(&bytes);
    CHECK(!x3pio_document_has_validity(x3pio_reader_document(reader)));
    x3pio_fields_init(&into);
    into.valid = (x3pio_field){flags, *packed(X3PIO_TYPE_UINT8)};
    CHECK_OK(x3pio_read_data(reader, &into, COUNT));
    for (size_t i = 0; i < COUNT; ++i)
    {
        CHECK(flags[i] == 1);
    }
    x3pio_reader_free(reader);
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);
}

/* An image of int16_t from a sensor, with its own marker for the points that
 * are missing, is written and read without a float in the memory of the
 * program. */
static void test_integers_stay_integers(void)
{
    int16_t image[COUNT];
    unsigned char mask[COUNT];
    for (size_t i = 0; i < COUNT; ++i)
    {
        image[i] = (int16_t)(1000 - 37 * (int)i);
        if (is_hole(i))
        {
            image[i] = INT16_MIN;
        }
        /* The program builds its mask from its marker in one pass. */
        mask[i] = image[i] != INT16_MIN ? 1 : 0;
    }
    x3pio_document *doc = grid_document(X3PIO_TYPE_INT16, 2.5e-9);
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    x3pio_const_fields from;
    x3pio_const_fields_init(&from);
    from.z = (x3pio_const_field){image, *packed(X3PIO_TYPE_INT16)};
    from.valid = (x3pio_const_field){mask, *packed(X3PIO_TYPE_UINT8)};
    CHECK_OK(x3pio_write_data(writer, &from, COUNT));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);

    x3pio_reader *reader = open_bytes(&bytes);
    int16_t back[COUNT];
    unsigned char flags[COUNT];
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.z = (x3pio_field){back, *packed(X3PIO_TYPE_INT16)};
    into.valid = (x3pio_field){flags, *packed(X3PIO_TYPE_UINT8)};
    CHECK_OK(x3pio_read_data(reader, &into, COUNT));
    CHECK(memcmp(back, image, sizeof image) == 0);
    CHECK(memcmp(flags, mask, sizeof mask) == 0);
    x3pio_reader_free(reader);

    /* The types of the program and of the file must be the same. */
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    uint16_t unsigned_image[COUNT] = {0};
    x3pio_const_fields_init(&from);
    from.z = (x3pio_const_field){unsigned_image, *packed(X3PIO_TYPE_UINT16)};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_data(writer, &from, COUNT));
    CHECK_STRING(
        x3pio_writer_error_message(writer),
        "The type of a layout is not the type in which the file stores "
        "the axis");
    x3pio_const_fields_init(&from);
    from.z = (x3pio_const_field){image, *packed(X3PIO_TYPE_INT16)};
    from.valid = (x3pio_const_field){mask, *packed(X3PIO_TYPE_INT16)};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_data(writer, &from, COUNT));
    /* A mask is not metres. */
    x3pio_const_fields_init(&from);
    from.z = (x3pio_const_field){image, *packed(X3PIO_TYPE_DOUBLE)};
    from.valid = (x3pio_const_field){mask, *packed(X3PIO_TYPE_UINT8)};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_points(writer, &from, COUNT));
    /* The writer is fine after the mistakes. */
    x3pio_const_fields_init(&from);
    from.z = (x3pio_const_field){image, *packed(X3PIO_TYPE_INT16)};
    CHECK_OK(x3pio_write_data(writer, &from, COUNT));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);
}

/* Absolute axes: a coordinate that is not a number makes the point missing. */
static void test_absolute_axes(void)
{
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(&doc));
    CHECK_OK(x3pio_document_set_feature_irregular_surface(
        doc, 3, 2, X3PIO_TYPE_FLOAT, 1.0, 1));
    double x[6] = {0, 1, 2, 0, 1, 2};
    double y[6] = {0, 0, 0, 1, 1, 1};
    double z[6] = {1, 2, 3, 4, 5, 6};
    x[1] = NAN;
    y[4] = INFINITY;
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    x3pio_const_fields from;
    x3pio_const_fields_init(&from);
    from.x = (x3pio_const_field){x, *packed(X3PIO_TYPE_DOUBLE)};
    from.y = (x3pio_const_field){y, *packed(X3PIO_TYPE_DOUBLE)};
    from.z = (x3pio_const_field){z, *packed(X3PIO_TYPE_DOUBLE)};
    CHECK_OK(x3pio_write_points(writer, &from, 6));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);

    x3pio_reader *reader = open_bytes(&bytes);
    double bx[6];
    double by[6];
    double bz[6];
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.x = (x3pio_field){bx, *packed(X3PIO_TYPE_DOUBLE)};
    into.y = (x3pio_field){by, *packed(X3PIO_TYPE_DOUBLE)};
    into.z = (x3pio_field){bz, *packed(X3PIO_TYPE_DOUBLE)};
    CHECK_OK(x3pio_read_points(reader, &into, 6));
    for (size_t i = 0; i < 6; ++i)
    {
        const bool hole = i == 1 || i == 4;
        CHECK(isnan(bz[i]) == hole);
        CHECK(isnan(bx[i]) == hole && isnan(by[i]) == hole);
    }
    x3pio_reader_free(reader);

    /* A point cloud has no missing points. */
    CHECK_OK(
        x3pio_document_set_feature_point_cloud(doc, 6, X3PIO_TYPE_DOUBLE, 1.0));
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_points(writer, &from, 6));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "A point cloud cannot hold missing points");
    /* The error ends the writing. */
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_write_points(writer, &from, 1));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_end_file(writer));
    x3pio_writer_free(writer);
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);
}

int main(void)
{
    test_floats();
    test_integer_mask();
    test_integer_value_of_missing_points();
    test_integers_stay_integers();
    test_absolute_axes();
    return finish_tests();
}
