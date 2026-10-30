/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Dates and times of the metadata.
 *
 * The calendar is computed here, with no function of the C library that has a
 * time zone or a result that threads share. Only x3pio_timestamp_now() asks the
 * C library for the zone of the machine.
 */

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "timestamp.h"
#include "text.h"
#include <time.h>

#define SECONDS_PER_MINUTE 60
#define MINUTES_PER_HOUR 60
#define HOURS_PER_DAY 24
#define SECONDS_PER_DAY                                                        \
    ((int64_t)SECONDS_PER_MINUTE * MINUTES_PER_HOUR * HOURS_PER_DAY)
#define MONTHS_PER_YEAR 12
#define MAX_YEAR 9999
#define MAX_OFFSET_MINUTES 840
#define MICROSECONDS 1000000
/* The proleptic Gregorian calendar. */
#define DAYS_PER_YEAR 365
#define DAYS_PER_400_YEARS (400 * DAYS_PER_YEAR + 97)
#define YEARS_PER_400_YEARS 400
/* From 0000-01-01 of the proleptic Gregorian calendar to 1970-01-01. */
#define YEAR_ZERO_TO_EPOCH 719528
#define FEBRUARY 2

/* The quotient rounded toward negative infinity, where C rounds toward 0. */
static int64_t floor_div(int64_t a, int64_t b)
{
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
    {
        --q;
    }
    return q;
}

static bool is_leap_year(int64_t year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static unsigned days_in_month(int64_t year, unsigned month)
{
    static const unsigned days[MONTHS_PER_YEAR] = {31, 28, 31, 30, 31, 30,
                                                   31, 31, 30, 31, 30, 31};
    return days[month - 1] + (month == FEBRUARY && is_leap_year(year) ? 1 : 0);
}

/* The days from 0000-01-01 to the first day of a year. */
static int64_t days_before_year(int64_t year)
{
    /* Year 0 is a leap year, which the formula counts with (year - 1) / 4 + 1.
     */
    const int64_t past = year - 1;
    return past * DAYS_PER_YEAR + floor_div(past, 4) - floor_div(past, 100)
           + floor_div(past, 400) + 366;
}

/* Days since 1970-01-01 of a date of the calendar. */
static int64_t days_from_civil(int64_t year, unsigned month, unsigned day)
{
    int64_t days =
        days_before_year(year) - YEAR_ZERO_TO_EPOCH + (int64_t)day - 1;
    for (unsigned before = 1; before < month; ++before)
    {
        days += days_in_month(year, before);
    }
    return days;
}

/* The date of a day count since 1970-01-01. */
static void civil_from_days(int64_t days, int64_t *year, unsigned *month,
                            unsigned *day)
{
    const int64_t since_zero = days + YEAR_ZERO_TO_EPOCH;
    /* The mean length of a year gives the year, or one off. */
    int64_t guess =
        floor_div(since_zero * YEARS_PER_400_YEARS, DAYS_PER_400_YEARS);
    while (days_before_year(guess) > since_zero)
    {
        --guess;
    }
    while (days_before_year(guess + 1) <= since_zero)
    {
        ++guess;
    }
    int64_t left = since_zero - days_before_year(guess);
    unsigned count = 1;
    while (left >= days_in_month(guess, count))
    {
        left -= days_in_month(guess, count);
        ++count;
    }
    *year = guess;
    *month = count;
    *day = (unsigned)left + 1;
}

bool x3pio_timestamp_valid(const x3pio_timestamp *t)
{
    if (t == NULL || t->year < 0 || t->year > MAX_YEAR || t->month < 1
        || t->month > MONTHS_PER_YEAR || t->day < 1
        || (unsigned)t->day > days_in_month(t->year, (unsigned)t->month)
        || t->hour < 0 || t->hour >= HOURS_PER_DAY || t->minute < 0
        || t->minute >= MINUTES_PER_HOUR || t->second < 0
        || t->second >= SECONDS_PER_MINUTE || t->microsecond < 0
        || t->microsecond >= MICROSECONDS)
    {
        return false;
    }
    return !t->has_offset
           || (t->utc_offset_minutes >= -MAX_OFFSET_MINUTES
               && t->utc_offset_minutes <= MAX_OFFSET_MINUTES);
}

/* Seconds as a time_t, if the type has room for them: a time_t of 32 bits ends
 * in the year 2038, and a date beyond it must not wrap into another one. */
static bool seconds_to_time(int64_t seconds, time_t *out)
{
    const time_t converted = (time_t)seconds;
    if ((int64_t)converted != seconds)
    {
        return false;
    }
    *out = converted;
    return true;
}

x3pio_status x3pio_timestamp_to_time(const x3pio_timestamp *timestamp,
                                     int assumed_offset_minutes, time_t *utc)
{
    if (utc == NULL || !x3pio_timestamp_valid(timestamp))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    const int64_t days = days_from_civil(
        timestamp->year, (unsigned)timestamp->month, (unsigned)timestamp->day);
    const int offset = timestamp->has_offset ? timestamp->utc_offset_minutes
                                             : assumed_offset_minutes;
    const int64_t seconds = days * SECONDS_PER_DAY
                            + ((int64_t)timestamp->hour * MINUTES_PER_HOUR
                               + timestamp->minute - offset)
                                  * SECONDS_PER_MINUTE
                            + timestamp->second;
    return seconds_to_time(seconds, utc) ? X3PIO_OK : X3PIO_ERR_RANGE;
}

x3pio_status x3pio_timestamp_from_time(time_t utc, int offset_minutes,
                                       x3pio_timestamp *timestamp)
{
    if (timestamp == NULL || offset_minutes < -MAX_OFFSET_MINUTES
        || offset_minutes > MAX_OFFSET_MINUTES)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    const int64_t wall =
        (int64_t)utc + (int64_t)offset_minutes * SECONDS_PER_MINUTE;
    const int64_t days = floor_div(wall, SECONDS_PER_DAY);
    const int64_t second_of_day = wall - days * SECONDS_PER_DAY;
    int64_t year = 0;
    unsigned month = 0;
    unsigned day = 0;
    civil_from_days(days, &year, &month, &day);
    if (year < 0 || year > MAX_YEAR)
    {
        return X3PIO_ERR_RANGE;
    }
    *timestamp = (x3pio_timestamp){
        .year = (int)year,
        .month = (int)month,
        .day = (int)day,
        .hour = (int)(second_of_day
                      / ((int64_t)SECONDS_PER_MINUTE * MINUTES_PER_HOUR)),
        .minute = (int)(second_of_day / SECONDS_PER_MINUTE % MINUTES_PER_HOUR),
        .second = (int)(second_of_day % SECONDS_PER_MINUTE),
        .microsecond = 0,
        .has_offset = true,
        .utc_offset_minutes = offset_minutes};
    return X3PIO_OK;
}

/* The fields of the local time of the machine. */
static bool local_fields(time_t utc, struct tm *fields)
{
#ifdef _WIN32
    return localtime_s(fields, &utc) == 0;
#else
    return localtime_r(&utc, fields) != NULL;
#endif
}

x3pio_status x3pio_timestamp_now(x3pio_timestamp *timestamp)
{
    if (timestamp == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    struct timespec now;
    if (timespec_get(&now, TIME_UTC) != TIME_UTC)
    {
        now.tv_sec = time(NULL);
        now.tv_nsec = 0;
    }
    if (now.tv_sec == (time_t)-1)
    {
        return X3PIO_ERR_RANGE;
    }
    struct tm local;
    if (!local_fields(now.tv_sec, &local))
    {
        return X3PIO_ERR_RANGE;
    }
    x3pio_timestamp wall = {.year = local.tm_year + 1900,
                            .month = local.tm_mon + 1,
                            .day = local.tm_mday,
                            .hour = local.tm_hour,
                            .minute = local.tm_min,
                            .second = local.tm_sec};
    time_t as_utc = 0;
    if (!x3pio_timestamp_valid(&wall)
        || x3pio_timestamp_to_time(&wall, 0, &as_utc) != X3PIO_OK)
    {
        return X3PIO_ERR_RANGE;
    }
    /* The zone is how far the wall is ahead of UTC. */
    const long difference = (long)((int64_t)as_utc - (int64_t)now.tv_sec);
    const int offset = (int)(difference / SECONDS_PER_MINUTE);
    if (offset < -MAX_OFFSET_MINUTES || offset > MAX_OFFSET_MINUTES)
    {
        return X3PIO_ERR_RANGE;
    }
    wall.microsecond = (int)(now.tv_nsec / 1000);
    wall.has_offset = true;
    wall.utc_offset_minutes = offset;
    *timestamp = wall;
    return X3PIO_OK;
}

/* Writes a number with at least `width` digits. */
static size_t put_digits(char *out, size_t used, size_t capacity,
                         unsigned value, size_t width)
{
    char digits[16];
    size_t count = 0;
    do
    {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0 && count < sizeof digits);
    while (count < width && count < sizeof digits)
    {
        digits[count++] = '0';
    }
    while (count > 0)
    {
        if (used + 1 < capacity)
        {
            out[used] = digits[count - 1];
        }
        ++used;
        --count;
    }
    return used;
}

static size_t put_char(char *out, size_t used, size_t capacity, char c)
{
    if (used + 1 < capacity)
    {
        out[used] = c;
    }
    return used + 1;
}

size_t x3pio_timestamp_format(const x3pio_timestamp *t, char *text,
                              size_t capacity)
{
    if (!x3pio_timestamp_valid(t) || text == NULL || capacity == 0)
    {
        return 0;
    }
    size_t used = 0;
    used = put_digits(text, used, capacity, (unsigned)t->year, 4);
    used = put_char(text, used, capacity, '-');
    used = put_digits(text, used, capacity, (unsigned)t->month, 2);
    used = put_char(text, used, capacity, '-');
    used = put_digits(text, used, capacity, (unsigned)t->day, 2);
    used = put_char(text, used, capacity, 'T');
    used = put_digits(text, used, capacity, (unsigned)t->hour, 2);
    used = put_char(text, used, capacity, ':');
    used = put_digits(text, used, capacity, (unsigned)t->minute, 2);
    used = put_char(text, used, capacity, ':');
    used = put_digits(text, used, capacity, (unsigned)t->second, 2);
    used = put_char(text, used, capacity, '.');
    /* The shortest fraction, with at least one digit. */
    unsigned fraction = (unsigned)t->microsecond;
    size_t width = 6;
    while (width > 1 && fraction % 10 == 0)
    {
        fraction /= 10;
        --width;
    }
    used = put_digits(text, used, capacity, fraction, width);
    if (t->has_offset)
    {
        const int offset = t->utc_offset_minutes;
        const unsigned magnitude = (unsigned)(offset < 0 ? -offset : offset);
        used = put_char(text, used, capacity, offset < 0 ? '-' : '+');
        used = put_digits(text, used, capacity, magnitude / 60, 2);
        used = put_char(text, used, capacity, ':');
        used = put_digits(text, used, capacity, magnitude % 60, 2);
    }
    text[used < capacity ? used : capacity - 1] = '\0';
    return used;
}

/* Reads exactly `digits` decimal digits. */
static bool read_digits(const char **text, size_t *size, size_t digits,
                        int *value)
{
    if (*size < digits)
    {
        return false;
    }
    int v = 0;
    for (size_t i = 0; i < digits; ++i)
    {
        if (!x3pio_is_digit((*text)[i]))
        {
            return false;
        }
        v = v * 10 + ((*text)[i] - '0');
    }
    *text += digits;
    *size -= digits;
    *value = v;
    return true;
}

static bool read_char(const char **text, size_t *size, char c)
{
    if (*size == 0 || (*text)[0] != c)
    {
        return false;
    }
    ++*text;
    --*size;
    return true;
}

bool x3pio_timestamp_parse(const char *text, x3pio_timestamp *timestamp)
{
    if (text == NULL || timestamp == NULL)
    {
        return false;
    }
    size_t size = strlen(text);
    x3pio_trim(&text, &size);
    x3pio_timestamp t = {0};
    if (!read_digits(&text, &size, 4, &t.year) || !read_char(&text, &size, '-')
        || !read_digits(&text, &size, 2, &t.month)
        || !read_char(&text, &size, '-')
        || !read_digits(&text, &size, 2, &t.day)
        || !read_char(&text, &size, 'T')
        || !read_digits(&text, &size, 2, &t.hour)
        || !read_char(&text, &size, ':')
        || !read_digits(&text, &size, 2, &t.minute)
        || !read_char(&text, &size, ':')
        || !read_digits(&text, &size, 2, &t.second))
    {
        return false;
    }
    if (read_char(&text, &size, '.'))
    {
        size_t digits = 0;
        int micro = 0;
        while (size > 0 && x3pio_is_digit(text[0]))
        {
            if (digits < 6)
            {
                micro = micro * 10 + (text[0] - '0');
            }
            ++digits;
            ++text;
            --size;
        }
        if (digits == 0)
        {
            return false;
        }
        for (size_t i = digits; i < 6; ++i)
        {
            micro *= 10;
        }
        t.microsecond = micro;
    }
    if (read_char(&text, &size, 'Z'))
    {
        t.has_offset = true;
    }
    else if (size > 0 && (text[0] == '+' || text[0] == '-'))
    {
        const bool negative = text[0] == '-';
        ++text;
        --size;
        int zone_hours = 0;
        int zone_minutes = 0;
        if (!read_digits(&text, &size, 2, &zone_hours)
            || !read_char(&text, &size, ':')
            || !read_digits(&text, &size, 2, &zone_minutes) || zone_hours > 14
            || zone_minutes > 59)
        {
            return false;
        }
        t.has_offset = true;
        t.utc_offset_minutes =
            (negative ? -1 : 1) * (zone_hours * 60 + zone_minutes);
    }
    if (size != 0 || !x3pio_timestamp_valid(&t))
    {
        return false;
    }
    *timestamp = t;
    return true;
}
