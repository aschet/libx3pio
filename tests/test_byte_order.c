/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The little-endian values of a file: fixed byte patterns, so the test holds on
 * a host of either byte order, and at any alignment. */

#include "byte_order.h"
#include "check.h"
#include "samples.h"

#include <string.h>

static void test_integers(void)
{
    const unsigned char b16[2] = {0x02, 0x01};
    const unsigned char b32[4] = {0x04, 0x03, 0x02, 0x01};
    const unsigned char b64[8] = {0x08, 0x07, 0x06, 0x05,
                                  0x04, 0x03, 0x02, 0x01};
    CHECK(x3pio_load_u16(b16) == 0x0102U);
    CHECK(x3pio_load_u32(b32) == 0x01020304UL);
    CHECK(x3pio_load_u64(b64) == 0x0102030405060708ULL);

    unsigned char out[10];
    memset(out, 0xAA, sizeof out);
    x3pio_store_u16(out + 1, 0x0102U);
    CHECK(memcmp(out + 1, b16, 2) == 0);
    CHECK(out[0] == 0xAA && out[3] == 0xAA);
    memset(out, 0xAA, sizeof out);
    x3pio_store_u32(out + 1, 0x01020304UL);
    CHECK(memcmp(out + 1, b32, 4) == 0);
    CHECK(out[0] == 0xAA && out[5] == 0xAA);
    memset(out, 0xAA, sizeof out);
    x3pio_store_u64(out + 1, 0x0102030405060708ULL);
    CHECK(memcmp(out + 1, b64, 8) == 0);
    CHECK(out[0] == 0xAA && out[9] == 0xAA);

    /* The extremes survive the trip. */
    unsigned char bytes[8];
    x3pio_store_u64(bytes, 0xFFFFFFFFFFFFFFFFULL);
    CHECK(x3pio_load_u64(bytes) == 0xFFFFFFFFFFFFFFFFULL);
    x3pio_store_u32(bytes, 0x80000000UL);
    CHECK(x3pio_load_u32(bytes) == 0x80000000UL);
    CHECK(bytes[3] == 0x80 && bytes[0] == 0);
}

static void test_floats(void)
{
    /* IEEE 754: 1.0f is 0x3F800000, -2.5f is 0xC0200000, 1.0 is
     * 0x3FF0000000000000 and -2.5 is 0xC004000000000000. */
    const unsigned char one_f[4] = {0x00, 0x00, 0x80, 0x3F};
    const unsigned char minus_f[4] = {0x00, 0x00, 0x20, 0xC0};
    const unsigned char one_d[8] = {0, 0, 0, 0, 0, 0, 0xF0, 0x3F};
    const unsigned char minus_d[8] = {0, 0, 0, 0, 0, 0, 0x04, 0xC0};
    CHECK(x3pio_load_float(one_f) == 1.0F);
    CHECK(x3pio_load_float(minus_f) == -2.5F);
    CHECK(x3pio_load_double(one_d) == 1.0);
    CHECK(x3pio_load_double(minus_d) == -2.5);

    unsigned char out[8];
    x3pio_store_float(out, 1.0F);
    CHECK(memcmp(out, one_f, 4) == 0);
    x3pio_store_float(out, -2.5F);
    CHECK(memcmp(out, minus_f, 4) == 0);
    x3pio_store_double(out, 1.0);
    CHECK(memcmp(out, one_d, 8) == 0);
    x3pio_store_double(out, -2.5);
    CHECK(memcmp(out, minus_d, 8) == 0);

    /* A NaN stays a NaN, an infinity an infinity, and a number is exact. */
    x3pio_store_float(out, NAN);
    CHECK(isnan(x3pio_load_float(out)));
    x3pio_store_double(out, -INFINITY);
    CHECK(isinf(x3pio_load_double(out)) && x3pio_load_double(out) < 0);
    x3pio_store_double(out, 0.1);
    CHECK(x3pio_load_double(out) == 0.1);
}

/* Reading at an odd address, as in a record that is not aligned. */
static void test_alignment(void)
{
    unsigned char buffer[16];
    for (size_t offset = 0; offset < 4; ++offset)
    {
        x3pio_store_double(buffer + offset, 123.456);
        CHECK(x3pio_load_double(buffer + offset) == 123.456);
        x3pio_store_u32(buffer + offset, 0xCAFEBABEUL);
        CHECK(x3pio_load_u32(buffer + offset) == 0xCAFEBABEUL);
    }
}

/* The samples of a file are these numbers, at the stride of a record. */
static void test_samples(void)
{
    /* Two records of an int16 and a float, 6 bytes each. */
    unsigned char records[12] = {0xFE, 0xFF, 0x00, 0x00, 0x80, 0x3F,
                                 0x02, 0x01, 0x00, 0x00, 0x20, 0xC0};
    double out[2];
    x3pio_decode_samples(X3PIO_TYPE_INT16, records, 6, out, 2);
    CHECK(out[0] == -2.0 && out[1] == 258.0);
    x3pio_decode_samples(X3PIO_TYPE_FLOAT, records + 2, 6, out, 2);
    CHECK(out[0] == 1.0 && out[1] == -2.5);

    unsigned char bytes[12] = {0};
    const double values[2] = {-2.0, 258.0};
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_INT16, values, 2, bytes, 6));
    CHECK(bytes[0] == 0xFE && bytes[1] == 0xFF && bytes[6] == 0x02
          && bytes[7] == 0x01);
    CHECK_OK(x3pio_encode_samples(X3PIO_TYPE_INT32, values, 2, bytes, 4));
    CHECK(bytes[0] == 0xFE && bytes[1] == 0xFF && bytes[2] == 0xFF
          && bytes[3] == 0xFF && bytes[4] == 0x02 && bytes[5] == 0x01
          && bytes[6] == 0x00 && bytes[7] == 0x00);

    /* A number outside the type is refused, and a type that is not stored too.
     */
    const double big = 40000.0;
    CHECK_STATUS(X3PIO_ERR_RANGE,
                 x3pio_encode_samples(X3PIO_TYPE_INT16, &big, 1, bytes, 2));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_encode_samples(X3PIO_TYPE_UINT8, &big, 1, bytes, 1));
}

int main(void)
{
    test_integers();
    test_floats();
    test_alignment();
    test_samples();
    return finish_tests();
}
