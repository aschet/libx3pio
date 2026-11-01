/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Numbers and dates as text, the same in every locale. */

#include "check.h"
#include "text.h"
#include "timestamp.h"

#include <float.h>
#include <locale.h>

static bool parse(const char *text, double *out)
{
    const x3pio_decimal_point point = x3pio_decimal_point_now();
    return x3pio_parse_number(&point, text, strlen(text), out);
}

static void test_parse(void)
{
    double v = 0;
    CHECK(parse("1.5", &v) && v == 1.5);
    CHECK(parse("  +2.5e-3 \n", &v) && equals(v, 2.5e-3));
    CHECK(parse("2.e-06", &v) && equals(v, 2e-6));
    CHECK(parse(".5", &v) && v == 0.5);
    CHECK(parse("-7", &v) && v == -7.0);
    CHECK(parse("1E3", &v) && v == 1000.0);
    CHECK(parse("inf", &v) && isinf(v) && v > 0);
    CHECK(parse("-Infinity", &v) && isinf(v) && v < 0);
    CHECK(parse("NaN", &v) && isnan(v));
    CHECK(parse("1e999", &v) && isinf(v));
    CHECK(parse("1e-999", &v) && v == 0.0);
    CHECK(!parse("", &v));
    CHECK(!parse("   ", &v));
    CHECK(!parse("+", &v));
    CHECK(!parse("--1", &v));
    CHECK(!parse("1e", &v));
    CHECK(!parse("1.2.3", &v));
    CHECK(!parse("0x10", &v));
    CHECK(!parse("1,5", &v));
    CHECK(!parse("1 2", &v));
    CHECK(!parse("abc", &v));
    CHECK(!parse("nan1", &v));
}

static void format_and_back(double value)
{
    char text[X3PIO_NUMBER_CAPACITY];
    const x3pio_decimal_point point = x3pio_decimal_point_now();
    const size_t size = x3pio_format_number(&point, value, text, sizeof text);
    CHECK(size == strlen(text));
    double back = 0;
    CHECK(x3pio_parse_number(&point, text, size, &back));
    if (isnan(value))
    {
        CHECK(isnan(back));
    }
    else
    {
        CHECK(back == value);
    }
    CHECK(strchr(text, ',') == NULL);
}

static void test_format(void)
{
    const x3pio_decimal_point point = x3pio_decimal_point_now();
    char text[X3PIO_NUMBER_CAPACITY];
    x3pio_format_number(&point, 0.1, text, sizeof text);
    CHECK_STRING(text, "0.1");
    x3pio_format_number(&point, 1e-6, text, sizeof text);
    CHECK_STRING(text, "1e-06");
    x3pio_format_number(&point, 1.0, text, sizeof text);
    CHECK_STRING(text, "1");
    x3pio_format_number(&point, -2.5, text, sizeof text);
    CHECK_STRING(text, "-2.5");
    x3pio_format_number(&point, INFINITY, text, sizeof text);
    CHECK_STRING(text, "inf");
    x3pio_format_number(&point, -INFINITY, text, sizeof text);
    CHECK_STRING(text, "-inf");
    x3pio_format_number(&point, NAN, text, sizeof text);
    CHECK_STRING(text, "nan");

    const double samples[] = {0.0,
                              -0.0,
                              1.0 / 3.0,
                              DBL_MAX,
                              DBL_MIN,
                              5e-324,
                              1e-300,
                              123456789.123456789,
                              0.1 + 0.2,
                              1e21,
                              9007199254740993.0,
                              -3.4028234663852886e38};
    for (size_t i = 0; i < sizeof samples / sizeof samples[0]; ++i)
    {
        format_and_back(samples[i]);
    }
    unsigned long long state = 88172645463325252ULL;
    for (int i = 0; i < 2000; ++i)
    {
        state ^= state << 13U;
        state ^= state >> 7U;
        state ^= state << 17U;
        double value = 0;
        memcpy(&value, &state, sizeof value);
        if (isfinite(value))
        {
            format_and_back(value);
        }
    }
}

static void test_locale(void)
{
    /* A locale with a decimal comma changes what the C library prints and
     * parses, not what the files hold. */
    const char *locales[] = {"de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8",
                             "de_DE", "de_AT.UTF-8"};
    const char *old = setlocale(LC_NUMERIC, NULL);
    char saved[64];
    snprintf(saved, sizeof saved, "%s", old != NULL ? old : "C");
    bool set = false;
    for (size_t i = 0; i < sizeof locales / sizeof locales[0] && !set; ++i)
    {
        set = setlocale(LC_NUMERIC, locales[i]) != NULL;
    }
    if (set)
    {
        const x3pio_decimal_point point = x3pio_decimal_point_now();
        CHECK(strcmp(point.text, ".") != 0);
        double v = 0;
        CHECK(parse("1.5", &v) && v == 1.5);
        CHECK(!parse("1,5", &v));
        char text[X3PIO_NUMBER_CAPACITY];
        x3pio_format_number(&point, 1.5, text, sizeof text);
        CHECK_STRING(text, "1.5");
        x3pio_format_number(&point, -0.000123, text, sizeof text);
        CHECK(strchr(text, ',') == NULL && strchr(text, '.') != NULL);
        format_and_back(1234.5678);
    }
    else
    {
        printf("no locale with a decimal comma: skipped\n");
    }
    setlocale(LC_NUMERIC, saved);
}

static void check_parsed(const char *text, int y, int mo, int d, int h, int mi,
                         int s, int us, bool has_offset, int offset)
{
    x3pio_timestamp t;
    CHECK(x3pio_timestamp_parse(text, &t));
    CHECK(t.year == y && t.month == mo && t.day == d && t.hour == h
          && t.minute == mi && t.second == s && t.microsecond == us);
    CHECK(t.has_offset == has_offset);
    CHECK(!has_offset || t.utc_offset_minutes == offset);
}

static void test_timestamp_text(void)
{
    check_parsed("2026-03-04T12:30:15", 2026, 3, 4, 12, 30, 15, 0, false, 0);
    check_parsed("2026-03-04T12:30:15.5", 2026, 3, 4, 12, 30, 15, 500000, false,
                 0);
    check_parsed("2026-03-04T12:30:15.123456789Z", 2026, 3, 4, 12, 30, 15,
                 123456, true, 0);
    check_parsed("  2026-03-04T12:30:15+01:00\n", 2026, 3, 4, 12, 30, 15, 0,
                 true, 60);
    check_parsed("2026-03-04T12:30:15.001-05:30", 2026, 3, 4, 12, 30, 15, 1000,
                 true, -330);
    check_parsed("2024-02-29T00:00:00", 2024, 2, 29, 0, 0, 0, 0, false, 0);
    check_parsed("0000-01-01T00:00:00", 0, 1, 1, 0, 0, 0, 0, false, 0);

    const char *bad[] = {"",
                         "N/A",
                         "Date of Calibration",
                         "2026-03-04",
                         "2026-03-04 12:30:15",
                         "2026-13-04T12:30:15",
                         "2026-02-30T12:30:15",
                         "2025-02-29T12:30:15",
                         "2026-03-04T24:30:15",
                         "2026-03-04T12:60:15",
                         "2026-03-04T12:30:60",
                         "2026-03-04T12:30:15.",
                         "2026-03-04T12:30:15+15:00",
                         "2026-03-04T12:30:15+01:60",
                         "2026-03-04T12:30:15+0100",
                         "2026-03-04T12:30:15Z extra",
                         "26-03-04T12:30:15"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
    {
        x3pio_timestamp t;
        CHECK(!x3pio_timestamp_parse(bad[i], &t));
    }

    char text[64];
    x3pio_timestamp t = {2026, 3, 4, 12, 30, 15, 500000, true, 60};
    CHECK(x3pio_timestamp_format(&t, text, sizeof text) == strlen(text));
    CHECK_STRING(text, "2026-03-04T12:30:15.5+01:00");
    t = (x3pio_timestamp){2026, 3, 4, 1, 2, 3, 0, false, 0};
    x3pio_timestamp_format(&t, text, sizeof text);
    CHECK_STRING(text, "2026-03-04T01:02:03.0");
    t = (x3pio_timestamp){12, 3, 4, 1, 2, 3, 120, true, -90};
    x3pio_timestamp_format(&t, text, sizeof text);
    CHECK_STRING(text, "0012-03-04T01:02:03.00012-01:30");
    t.month = 13;
    CHECK(x3pio_timestamp_format(&t, text, sizeof text) == 0);
    CHECK(x3pio_timestamp_format(&t, NULL, 0) == 0);

    /* A text that is cut off reports the length it needs. */
    t = (x3pio_timestamp){2026, 3, 4, 12, 30, 15, 500000, true, 60};
    CHECK(x3pio_timestamp_format(&t, text, 8) == 27);
    CHECK(strlen(text) == 7);

    /* Formatted timestamps read back. */
    for (int us = 0; us < 1000000; us += 12345)
    {
        t = (x3pio_timestamp){1999, 12, 31, 23, 59, 59, us, us % 2 == 0, 540};
        x3pio_timestamp_format(&t, text, sizeof text);
        x3pio_timestamp back;
        CHECK(x3pio_timestamp_parse(text, &back));
        CHECK(back.has_offset == t.has_offset
              && (!t.has_offset
                  || back.utc_offset_minutes == t.utc_offset_minutes));
        CHECK(back.microsecond == t.microsecond && back.second == t.second);
    }
}

static void test_timestamp_time(void)
{
    time_t utc = 0;
    x3pio_timestamp t = {1970, 1, 1, 0, 0, 0, 0, true, 0};
    CHECK_OK(x3pio_timestamp_to_time(&t, 0, &utc));
    CHECK(utc == 0);
    t = (x3pio_timestamp){1970, 1, 1, 1, 0, 0, 0, true, 60};
    CHECK_OK(x3pio_timestamp_to_time(&t, 0, &utc));
    CHECK(utc == 0);
    t = (x3pio_timestamp){2026, 3, 4, 12, 30, 15, 999999, true, 60};
    CHECK_OK(x3pio_timestamp_to_time(&t, 0, &utc));
    CHECK((long long)utc == 1772623815LL);
    /* A timestamp without a zone takes the assumed one. */
    t.has_offset = false;
    CHECK_OK(x3pio_timestamp_to_time(&t, 120, &utc));
    CHECK((long long)utc == 1772623815LL - 3600);
    CHECK_OK(x3pio_timestamp_to_time(&t, 0, &utc));
    CHECK((long long)utc == 1772623815LL + 3600);

    x3pio_timestamp back;
    CHECK_OK(x3pio_timestamp_from_time((time_t)1772623815LL, 60, &back));
    CHECK(back.year == 2026 && back.month == 3 && back.day == 4
          && back.hour == 12 && back.minute == 30 && back.second == 15);
    CHECK(back.has_offset && back.utc_offset_minutes == 60);
    CHECK_OK(x3pio_timestamp_from_time(0, -300, &back));
    CHECK(back.year == 1969 && back.month == 12 && back.day == 31
          && back.hour == 19);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_timestamp_from_time(0, 900, &back));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_timestamp_from_time(0, 0, NULL));

    /* Every day of several years converts both ways. */
    for (int day = -800; day <= 800; ++day)
    {
        const time_t seconds = (time_t)day * 86400 + 3661;
        CHECK_OK(x3pio_timestamp_from_time(seconds, 0, &back));
        time_t again = 0;
        CHECK_OK(x3pio_timestamp_to_time(&back, 0, &again));
        CHECK(again == seconds);
    }
    for (int year = 0; year <= 9999; year += 37)
    {
        t = (x3pio_timestamp){year, 2, 28, 23, 59, 58, 0, true, 0};
        time_t seconds = 0;
        const x3pio_status status = x3pio_timestamp_to_time(&t, 0, &seconds);
        if (status == X3PIO_OK)
        {
            CHECK_OK(x3pio_timestamp_from_time(seconds, 0, &back));
            CHECK(back.year == year && back.month == 2 && back.day == 28);
        }
        else
        {
            CHECK(status == X3PIO_ERR_RANGE);
        }
    }
    t = (x3pio_timestamp){2026, 2, 30, 0, 0, 0, 0, false, 0};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_timestamp_to_time(&t, 0, &utc));

    /* The clock gives a plausible date. */
    x3pio_timestamp now;
    CHECK_OK(x3pio_timestamp_now(&now));
    CHECK(now.year >= 2026 && now.has_offset);
    CHECK(x3pio_timestamp_valid(&now));
    time_t from_clock = 0;
    CHECK_OK(x3pio_timestamp_to_time(&now, 0, &from_clock));
    CHECK(llabs((long long)from_clock - (long long)time(NULL)) < 5);
}

static void test_utf8(void)
{
    CHECK(x3pio_utf8_valid("plain", 5));
    CHECK(x3pio_utf8_valid("\xC3\xA4\xE2\x82\xAC\xF0\x9F\x98\x80", 9));
    CHECK(x3pio_utf8_valid("", 0));
    CHECK(!x3pio_utf8_valid("\xC3", 1));
    CHECK(!x3pio_utf8_valid("\xC0\xAF", 2));
    CHECK(!x3pio_utf8_valid("\xED\xA0\x80", 3));
    CHECK(!x3pio_utf8_valid("\xF4\x90\x80\x80", 4));
    CHECK(!x3pio_utf8_valid("\xFF", 1));
    CHECK(!x3pio_utf8_valid("\xE0\x80\x80", 3));
}

int main(void)
{
    test_parse();
    test_format();
    test_locale();
    test_timestamp_text();
    test_timestamp_time();
    test_utf8();
    return finish_tests();
}
