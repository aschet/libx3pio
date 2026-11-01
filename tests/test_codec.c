/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The raw values of a file, and the views of the memory of the program:
 * strided, negative, members of structures, and every type a view can hold. */

#include "check.h"
#include "matrix.h"
#include "samples.h"

#include <float.h>
#include <stddef.h>

static void test_little_endian(void)
{
    const unsigned char bytes[] = {0x01, 0x02, 0x03, 0x04,
                                   0x05, 0x06, 0x07, 0x08};
    double out[2];
    x3pio_decode_samples(X3PIO_TYPE_INT16, bytes, 2, out, 2);
    CHECK(out[0] == 0x0201 && out[1] == 0x0403);
    x3pio_decode_samples(X3PIO_TYPE_INT32, bytes, 4, out, 2);
    CHECK(out[0] == 0x04030201 && out[1] == 0x08070605);
    const unsigned char negative[] = {0xFF, 0xFF, 0x00, 0x80};
    x3pio_decode_samples(X3PIO_TYPE_INT16, negative, 2, out, 2);
    CHECK(out[0] == -1.0 && out[1] == -32768.0);
    x3pio_decode_samples(X3PIO_TYPE_INT32, negative, 4, out, 1);
    CHECK(out[0] == (double)(int32_t)0x8000FFFF);
    const unsigned char one_float[] = {0x00, 0x00, 0x80, 0x3F};
    x3pio_decode_samples(X3PIO_TYPE_FLOAT, one_float, 4, out, 1);
    CHECK(out[0] == 1.0);
    const unsigned char one_double[] = {0, 0, 0, 0, 0, 0, 0xF0, 0x3F};
    x3pio_decode_samples(X3PIO_TYPE_DOUBLE, one_double, 8, out, 1);
    CHECK(out[0] == 1.0);

    unsigned char encoded[8];
    const double values[] = {1.0};
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_FLOAT, values, 1, encoded, 4));
    CHECK(memcmp(encoded, one_float, 4) == 0);
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_DOUBLE, values, 1, encoded, 8));
    CHECK(memcmp(encoded, one_double, 8) == 0);
    const double ints[] = {0x0201};
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_INT16, ints, 1, encoded, 2));
    CHECK(encoded[0] == 0x01 && encoded[1] == 0x02);
}

static void test_stride(void)
{
    /* Records of an int16 and a float, read one axis at a time. */
    unsigned char records[3 * 6];
    for (size_t i = 0; i < 3; ++i)
    {
        const double a[] = {(double)i - 1.0};
        const double b[] = {0.5 * (double)(i + 1)};
        CHECK_OK(
            x3pio_encode_samples(X3PIO_TYPE_INT16, a, 1, records + i * 6, 6));
        CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_FLOAT, b, 1,
                                      records + i * 6 + 2, 6));
    }
    double ints[3];
    double floats[3];
    x3pio_decode_samples(X3PIO_TYPE_INT16, records, 6, ints, 3);
    x3pio_decode_samples(X3PIO_TYPE_FLOAT, records + 2, 6, floats, 3);
    for (size_t i = 0; i < 3; ++i)
    {
        CHECK(ints[i] == (double)i - 1.0);
        CHECK(floats[i] == 0.5 * (double)(i + 1));
    }
}

static void test_round_trip_of_types(void)
{
    const x3pio_type types[] = {X3PIO_TYPE_INT16, X3PIO_TYPE_INT32,
                                X3PIO_TYPE_FLOAT, X3PIO_TYPE_DOUBLE};
    const double values[] = {0.0, 1.0, -1.0, 100.0, -32768.0, 32767.0, 1234.0};
    for (size_t t = 0; t < 4; ++t)
    {
        unsigned char bytes[7 * 8];
        const size_t size = x3pio_type_size(types[t]);
        CHECK_OK(x3pio_encode_samples(types[t], values, 7, bytes, size));
        double back[7];
        x3pio_decode_samples(types[t], bytes, size, back, 7);
        for (size_t i = 0; i < 7; ++i)
        {
            CHECK(back[i] == values[i]);
        }
    }
    /* A float keeps what it can hold and rounds the rest. */
    unsigned char bytes[8];
    const double fine[] = {0.1};
    double back = 0;
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_FLOAT, fine, 1, bytes, 4));
    x3pio_decode_samples(X3PIO_TYPE_FLOAT, bytes, 4, &back, 1);
    const float tenth = 0.1F;
    CHECK(equals(back, (double)tenth));
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_DOUBLE, fine, 1, bytes, 8));
    x3pio_decode_samples(X3PIO_TYPE_DOUBLE, bytes, 8, &back, 1);
    CHECK(equals(back, 0.1));
}

static void test_rounding(void)
{
    const double in[] = {0.5,  1.5,  2.5,  -0.5,     -1.5,
                         -2.5, 0.49, 0.51, 1e3 + 0.5};
    const double expect[] = {0, 2, 2, 0, -2, -2, 0, 1, 1000};
    unsigned char bytes[9 * 2];
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_INT16, in, 9, bytes, 2));
    double back[9];
    x3pio_decode_samples(X3PIO_TYPE_INT16, bytes, 2, back, 9);
    for (size_t i = 0; i < 9; ++i)
    {
        CHECK(back[i] == expect[i]);
    }
}

static void test_range(void)
{
    unsigned char bytes[8];
    double value = 32768.0;
    CHECK_STATUS(X3PIO_ERR_RANGE,
                 x3pio_encode_samples(X3PIO_TYPE_INT16, &value, 1, bytes, 2));
    value = -32769.0;
    CHECK_STATUS(X3PIO_ERR_RANGE,
                 x3pio_encode_samples(X3PIO_TYPE_INT16, &value, 1, bytes, 2));
    value = 32767.4;
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_INT16, &value, 1, bytes, 2));
    value = 2147483648.0;
    CHECK_STATUS(X3PIO_ERR_RANGE,
                 x3pio_encode_samples(X3PIO_TYPE_INT32, &value, 1, bytes, 4));
    value = -2147483648.0;
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_INT32, &value, 1, bytes, 4));
    value = 1e39;
    CHECK_STATUS(X3PIO_ERR_RANGE,
                 x3pio_encode_samples(X3PIO_TYPE_FLOAT, &value, 1, bytes, 4));
    value = (double)FLT_MAX;
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_FLOAT, &value, 1, bytes, 4));
    value = INFINITY;
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_FLOAT, &value, 1, bytes, 4));
    value = INFINITY;
    CHECK_STATUS(X3PIO_ERR_RANGE,
                 x3pio_encode_samples(X3PIO_TYPE_INT16, &value, 1, bytes, 2));
    /* A NaN is 0 for an integer type and stays NaN for a float type. */
    value = NAN;
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_INT16, &value, 1, bytes, 2));
    double back = 7;
    x3pio_decode_samples(X3PIO_TYPE_INT16, bytes, 2, &back, 1);
    CHECK(back == 0.0);
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_FLOAT, &value, 1, bytes, 4));
    x3pio_decode_samples(X3PIO_TYPE_FLOAT, bytes, 4, &back, 1);
    CHECK(isnan(back));
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_DOUBLE, &value, 1, bytes, 8));
    x3pio_decode_samples(X3PIO_TYPE_DOUBLE, bytes, 8, &back, 1);
    CHECK(isnan(back));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_encode_samples(X3PIO_TYPE_UINT8, &value, 1, bytes, 1));
}

/* A matrix of the buffer with the given layout. */
static x3pio_matrix matrix_of(const void *data, x3pio_type type,
                              ptrdiff_t element_stride, ptrdiff_t row_stride,
                              size_t columns, size_t rows)
{
    x3pio_matrix matrix;
    const x3pio_layout layout = {type, element_stride, row_stride};
    CHECK(x3pio_matrix_init(&matrix, data, &layout, columns, rows, true));
    return matrix;
}

static void test_layouts(void)
{
    /* An image of 3 rows by 4 columns of doubles, read in pieces. */
    double image[12];
    for (size_t i = 0; i < 12; ++i)
    {
        image[i] = (double)i;
    }
    x3pio_matrix m = matrix_of(image, X3PIO_TYPE_DOUBLE, 0, 0, 4, 3);
    CHECK(m.element_stride == 8 && m.row_stride == 32);
    double out[5];
    x3pio_matrix_load(&m, 3, out, 5);
    for (size_t i = 0; i < 5; ++i)
    {
        CHECK(out[i] == (double)(3 + i));
    }

    /* Rows apart by more than a row: an image with padding. */
    double padded[3 * 6] = {0};
    for (size_t r = 0; r < 3; ++r)
    {
        for (size_t col = 0; col < 4; ++col)
        {
            padded[r * 6 + col] = (double)(r * 10 + col);
        }
    }
    m = matrix_of(padded, X3PIO_TYPE_DOUBLE, 0, 6 * sizeof(double), 4, 3);
    double all[12];
    x3pio_matrix_load(&m, 0, all, 12);
    for (size_t r = 0; r < 3; ++r)
    {
        for (size_t col = 0; col < 4; ++col)
        {
            CHECK(all[r * 4 + col] == (double)(r * 10 + col));
        }
    }

    /* A matrix upside down: the last row first, by a negative row stride. */
    m = matrix_of(image + 8, X3PIO_TYPE_DOUBLE, 0, -32, 4, 3);
    x3pio_matrix_load(&m, 0, all, 12);
    CHECK(all[0] == 8.0 && all[3] == 11.0 && all[4] == 4.0 && all[8] == 0.0);

    /* A matrix mirrored in the other direction. */
    m = matrix_of(image + 3, X3PIO_TYPE_DOUBLE, -8, 32, 4, 3);
    x3pio_matrix_load(&m, 0, all, 12);
    CHECK(all[0] == 3.0 && all[3] == 0.0 && all[4] == 7.0);

    /* A column-major matrix: the elements of a row are a column apart. */
    m = matrix_of(image, X3PIO_TYPE_DOUBLE, 3 * 8, 8, 4, 3);
    x3pio_matrix_load(&m, 0, all, 12);
    CHECK(all[0] == 0.0 && all[1] == 3.0 && all[4] == 1.0 && all[11] == 11.0);

    /* A member of an array of structures. */
    // NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
    struct point
    {
        char tag;
        float x;
        short y;
        double z;
    } points[4];
    for (size_t i = 0; i < 4; ++i)
    {
        points[i] =
            (struct point){'p', (float)i, (short)(-(int)i), 10.0 * (double)i};
    }
    m = matrix_of(&points[0].z, X3PIO_TYPE_DOUBLE, sizeof points[0], 0, 4, 1);
    x3pio_matrix_load(&m, 0, all, 4);
    CHECK(all[0] == 0.0 && all[1] == 10.0 && all[3] == 30.0);
    m = matrix_of(&points[0].y, X3PIO_TYPE_INT16, sizeof points[0], 0, 4, 1);
    x3pio_matrix_load(&m, 0, all, 4);
    CHECK(all[2] == -2.0);
    m = matrix_of(&points[0].x, X3PIO_TYPE_FLOAT, sizeof points[0], 0, 4, 1);
    x3pio_matrix_load(&m, 1, all, 3);
    CHECK(all[0] == 1.0 && all[2] == 3.0);

    /* Written in place, the neighbours stay as they are. */
    m = matrix_of(&points[0].y, X3PIO_TYPE_INT16, sizeof points[0], 0, 4, 1);
    const double ys[] = {5, 6, 7, 8};
    CHECK_OK(x3pio_matrix_store(&m, 0, ys, 4));
    for (size_t i = 0; i < 4; ++i)
    {
        CHECK(points[i].y == (short)(5 + i) && points[i].tag == 'p'
              && points[i].x == (float)i && points[i].z == 10.0 * (double)i);
    }

    /* A channel of an interleaved image of 2 by 3 pixels of 3 bytes. */
    unsigned char rgb[2 * 3 * 3] = {0};
    m = matrix_of(rgb + 1, X3PIO_TYPE_UINT8, 3, 9, 3, 2);
    const double green[] = {10, 20, 30, 40, 50, 60};
    CHECK_OK(x3pio_matrix_store(&m, 0, green, 6));
    CHECK(rgb[1] == 10 && rgb[4] == 20 && rgb[7] == 30 && rgb[10] == 40
          && rgb[16] == 60 && rgb[0] == 0 && rgb[2] == 0);

    /* A piece that starts in the middle of a row and goes over the end of it.
     */
    double piece[7];
    m = matrix_of(image, X3PIO_TYPE_DOUBLE, 0, 0, 4, 3);
    x3pio_matrix_load(&m, 2, piece, 7);
    for (size_t i = 0; i < 7; ++i)
    {
        CHECK(piece[i] == (double)(2 + i));
    }
    m = matrix_of(padded, X3PIO_TYPE_DOUBLE, 0, 6 * sizeof(double), 4, 3);
    x3pio_matrix_load(&m, 2, piece, 7);
    CHECK(piece[0] == 2.0 && piece[1] == 3.0 && piece[2] == 10.0
          && piece[5] == 13.0 && piece[6] == 20.0);
}

static void test_layout_checks(void)
{
    double d[4] = {0};
    x3pio_matrix m;
    const x3pio_layout packed = {X3PIO_TYPE_DOUBLE, 0, 0};
    CHECK(x3pio_matrix_init(&m, d, &packed, 4, 1, true));
    CHECK(!x3pio_matrix_init(&m, NULL, &packed, 4, 1, true));
    CHECK(!x3pio_matrix_init(&m, d, &packed, 0, 1, true));
    CHECK(!x3pio_matrix_init(&m, d, &packed, 4, 0, true));
    const x3pio_layout bad_type = {(x3pio_type)42, 0, 0};
    CHECK(!x3pio_matrix_init(&m, d, &bad_type, 4, 1, true));
    /* The default row stride must not overflow. */
    CHECK(!x3pio_matrix_init(&m, d, &packed, SIZE_MAX / 4, 2, true));

    CHECK(x3pio_layout_packed(X3PIO_TYPE_DOUBLE)->type == X3PIO_TYPE_DOUBLE);
    CHECK(x3pio_layout_packed(X3PIO_TYPE_INT8)->type == X3PIO_TYPE_INT8);
    CHECK(x3pio_layout_packed((x3pio_type)42) == NULL);
    for (int t = 0; t < 8; ++t)
    {
        const x3pio_layout *layout = x3pio_layout_packed((x3pio_type)t);
        CHECK(layout != NULL && layout->type == (x3pio_type)t
              && layout->element_stride == 0 && layout->row_stride == 0);
    }
    x3pio_layout layout;
    x3pio_layout_init(&layout);
    CHECK(layout.type == X3PIO_TYPE_DOUBLE && layout.element_stride == 0);
    x3pio_fields fields;
    x3pio_fields_init(&fields);
    CHECK(fields.x.data == NULL && fields.valid.data == NULL);
}

static void test_conversions(void)
{
    const x3pio_type types[] = {X3PIO_TYPE_INT8,  X3PIO_TYPE_UINT8,
                                X3PIO_TYPE_INT16, X3PIO_TYPE_UINT16,
                                X3PIO_TYPE_INT32, X3PIO_TYPE_UINT32,
                                X3PIO_TYPE_FLOAT, X3PIO_TYPE_DOUBLE};
    const double samples[] = {0.0, 1.0, 100.0, 42.0};
    for (size_t t = 0; t < 8; ++t)
    {
        unsigned char memory[4 * 8];
        x3pio_matrix m = matrix_of(memory, types[t], 0, 0, 4, 1);
        CHECK_OK(x3pio_matrix_store(&m, 0, samples, 4));
        double back[4];
        x3pio_matrix_load(&m, 0, back, 4);
        for (size_t i = 0; i < 4; ++i)
        {
            CHECK(back[i] == samples[i]);
        }
    }
    /* The limits of each type. */
    unsigned char b[8];
    double value = 128.0;
    x3pio_matrix m = matrix_of(b, X3PIO_TYPE_INT8, 0, 0, 1, 1);
    CHECK_STATUS(X3PIO_ERR_RANGE, x3pio_matrix_store(&m, 0, &value, 1));
    value = -128.0;
    CHECK_OK(x3pio_matrix_store(&m, 0, &value, 1));
    m = matrix_of(b, X3PIO_TYPE_UINT8, 0, 0, 1, 1);
    value = -1.0;
    CHECK_STATUS(X3PIO_ERR_RANGE, x3pio_matrix_store(&m, 0, &value, 1));
    value = 255.0;
    CHECK_OK(x3pio_matrix_store(&m, 0, &value, 1));
    value = 256.0;
    CHECK_STATUS(X3PIO_ERR_RANGE, x3pio_matrix_store(&m, 0, &value, 1));
    m = matrix_of(b, X3PIO_TYPE_UINT16, 0, 0, 1, 1);
    value = 65535.0;
    CHECK_OK(x3pio_matrix_store(&m, 0, &value, 1));
    value = 65536.0;
    CHECK_STATUS(X3PIO_ERR_RANGE, x3pio_matrix_store(&m, 0, &value, 1));
    m = matrix_of(b, X3PIO_TYPE_UINT32, 0, 0, 1, 1);
    value = 4294967295.0;
    CHECK_OK(x3pio_matrix_store(&m, 0, &value, 1));
    value = 4294967296.0;
    CHECK_STATUS(X3PIO_ERR_RANGE, x3pio_matrix_store(&m, 0, &value, 1));
    /* A NaN is 0 for an integer buffer. */
    value = NAN;
    CHECK_OK(x3pio_matrix_store(&m, 0, &value, 1));
    double back = 9;
    x3pio_matrix_load(&m, 0, &back, 1);
    CHECK(back == 0.0);
    /* A NaN and an infinity stay in a buffer of floats. */
    float f[2];
    m = matrix_of(f, X3PIO_TYPE_FLOAT, 0, 0, 2, 1);
    const double specials[] = {NAN, -INFINITY};
    CHECK_OK(x3pio_matrix_store(&m, 0, specials, 2));
    CHECK(isnan(f[0]) && isinf(f[1]) && f[1] < 0);
    value = 1e39;
    CHECK_STATUS(X3PIO_ERR_RANGE, x3pio_matrix_store(&m, 0, &value, 1));
}

static void test_validity_bits(void)
{
    /* The point j is bit j % 8 of byte j / 8, least significant first. */
    unsigned char flags[11] = {1, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1};
    unsigned char bits[2];
    memset(bits, 0xAA, sizeof bits);
    x3pio_validity_pack(flags, 11, bits);
    CHECK(bits[0] == 0x8D); /* 1 0 1 1 0 0 0 1 */
    CHECK(bits[1] == 0x05); /* 1 0 1, the rest of the byte is 0 */
    unsigned char back[11];
    x3pio_validity_unpack(bits, 11, back);
    CHECK(memcmp(back, flags, sizeof flags) == 0);
    /* Any nonzero byte is a point that was measured. */
    unsigned char loose[3] = {7, 0, 255};
    x3pio_validity_pack(loose, 3, bits);
    CHECK(bits[0] == 0x05);
    x3pio_validity_pack(loose, 0, bits);
}

int main(void)
{
    test_little_endian();
    test_stride();
    test_round_trip_of_types();
    test_rounding();
    test_range();
    test_layouts();
    test_layout_checks();
    test_conversions();
    test_validity_bits();
    return finish_tests();
}
