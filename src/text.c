/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Characters, numbers and strings, independent of the locale.
 *
 * The functions of the C library that print and parse numbers use the decimal
 * point of the current locale, whereas the files always use a point. The
 * functions here translate it.
 */

#include "text.h"
#include <locale.h>
#include <math.h>

void x3pio_trim(const char **text, size_t *size)
{
    while (*size > 0 && x3pio_is_space((*text)[0]))
    {
        ++*text;
        --*size;
    }
    while (*size > 0 && x3pio_is_space((*text)[*size - 1]))
    {
        --*size;
    }
}

bool x3pio_equals_nocase(const char *text, size_t size, const char *other)
{
    if (strlen(other) != size)
    {
        return false;
    }
    for (size_t i = 0; i < size; ++i)
    {
        if (x3pio_to_lower(text[i]) != other[i])
        {
            return false;
        }
    }
    return true;
}

char *x3pio_copy_text(const x3pio_allocator *allocator, const char *text,
                      size_t size)
{
    char *copy = x3pio_allocate(allocator, size + 1);
    if (copy != NULL)
    {
        if (size > 0)
        {
            memcpy(copy, text, size);
        }
        copy[size] = '\0';
    }
    return copy;
}

char *x3pio_strdup(const x3pio_allocator *allocator, const char *text)
{
    return x3pio_copy_text(allocator, text, strlen(text));
}

x3pio_decimal_point x3pio_decimal_point_now(void)
{
    x3pio_decimal_point decimal_point = {{0}};
    decimal_point.text[0] = '.';
    char probe[32];
    const int printed = snprintf(probe, sizeof probe, "%.1f", 0.5);
    /* "0" + point + "5": whatever lies between the digits is the point. */
    if (printed >= 3 && (size_t)printed < sizeof probe
        && (size_t)printed - 2 < sizeof decimal_point.text)
    {
        const size_t length = (size_t)printed - 2;
        memcpy(decimal_point.text, probe + 1, length);
        decimal_point.text[length] = '\0';
    }
    return decimal_point;
}

/* Checks the text against the grammar of a decimal number and copies it into a
 * buffer with the decimal point of the locale, which strtod() wants. The sign
 * is not copied. */
static bool scan_decimal(const x3pio_decimal_point *decimal_point,
                         const char *text, size_t size, char *buffer,
                         size_t capacity, size_t *length)
{
    /* digits [. digits] [e [sign] digits], with a digit before or after the
     * dot */
    size_t i = 0;
    size_t digits = 0;
    while (i < size && x3pio_is_digit(text[i]))
    {
        ++i;
        ++digits;
    }
    size_t dot = size;
    if (i < size && text[i] == '.')
    {
        dot = i;
        ++i;
        while (i < size && x3pio_is_digit(text[i]))
        {
            ++i;
            ++digits;
        }
    }
    if (digits == 0)
    {
        return false;
    }
    if (i < size && (text[i] == 'e' || text[i] == 'E'))
    {
        ++i;
        if (i < size && (text[i] == '+' || text[i] == '-'))
        {
            ++i;
        }
        size_t exponent = 0;
        while (i < size && x3pio_is_digit(text[i]))
        {
            ++i;
            ++exponent;
        }
        if (exponent == 0)
        {
            return false;
        }
    }
    if (i != size)
    {
        return false;
    }
    const size_t point_size = strlen(decimal_point->text);
    if (size + point_size >= capacity)
    {
        return false;
    }
    *length = 0;
    for (size_t k = 0; k < size; ++k)
    {
        if (k == dot)
        {
            memcpy(buffer + *length, decimal_point->text, point_size);
            *length += point_size;
        }
        else
        {
            buffer[*length] = text[k];
            ++*length;
        }
    }
    buffer[*length] = '\0';
    return true;
}

bool x3pio_parse_number(const x3pio_decimal_point *decimal_point,
                        const char *text, size_t size, double *out)
{
    x3pio_trim(&text, &size);
    bool negative = false;
    if (size > 0 && (text[0] == '+' || text[0] == '-'))
    {
        negative = text[0] == '-';
        ++text;
        --size;
    }
    if (size == 0 || text[0] == '+' || text[0] == '-')
    {
        return false;
    }
    if (x3pio_equals_nocase(text, size, "inf")
        || x3pio_equals_nocase(text, size, "infinity"))
    {
        *out = negative ? -INFINITY : INFINITY;
        return true;
    }
    if (x3pio_equals_nocase(text, size, "nan"))
    {
        *out = NAN;
        return true;
    }
    char buffer[X3PIO_NUMBER_CAPACITY * 2];
    size_t length = 0;
    if (!scan_decimal(decimal_point, text, size, buffer, sizeof buffer,
                      &length))
    {
        return false;
    }
    char *end = NULL;
    const double value = strtod(buffer, &end);
    if (end != buffer + length)
    {
        return false;
    }
    /* An overflow gives infinity and an underflow zero, as strtod() does. */
    *out = negative ? -value : value;
    return true;
}

size_t x3pio_format_number(const x3pio_decimal_point *decimal_point,
                           double value, char *out, size_t capacity)
{
    if (isnan(value) != 0)
    {
        memcpy(out, "nan", 4);
        return 3;
    }
    if (isinf(value) != 0)
    {
        const char *text = value < 0 ? "-inf" : "inf";
        const size_t size = strlen(text);
        memcpy(out, text, size + 1);
        return size;
    }
    /* A shorter text is tried first; the first that reads back exactly wins. */
    char raw[X3PIO_NUMBER_CAPACITY];
    int printed = 0;
    for (int digits = 15; digits <= 17; ++digits)
    {
        printed = snprintf(raw, sizeof raw, "%.*g", digits, value);
        if (printed < 0 || (size_t)printed >= sizeof raw)
        {
            out[0] = '0';
            out[1] = '\0';
            return 1;
        }
        if (strtod(raw, NULL) == value)
        {
            break;
        }
    }
    /* Put the point of the file where the locale has its own. */
    const size_t point_size = strlen(decimal_point->text);
    size_t used = 0;
    size_t i = 0;
    const size_t length = (size_t)printed;
    while (i < length && used + 1 < capacity)
    {
        if (point_size > 0 && i + point_size <= length && raw[i] != '.'
            && memcmp(raw + i, decimal_point->text, point_size) == 0
            && !x3pio_is_digit(raw[i]) && raw[i] != 'e' && raw[i] != '-'
            && raw[i] != '+')
        {
            out[used++] = '.';
            i += point_size;
        }
        else
        {
            out[used++] = raw[i++];
        }
    }
    out[used] = '\0';
    return used;
}

bool x3pio_utf8_valid(const char *text, size_t size)
{
    size_t i = 0;
    while (i < size)
    {
        const unsigned char c = (unsigned char)text[i];
        size_t extra = 0;
        uint32_t code = 0;
        if (c < 0x80)
        {
            code = c;
        }
        else if (c >= 0xC2 && c <= 0xDF)
        {
            extra = 1;
            code = c & 0x1FU;
        }
        else if (c >= 0xE0 && c <= 0xEF)
        {
            extra = 2;
            code = c & 0x0FU;
        }
        else if (c >= 0xF0 && c <= 0xF4)
        {
            extra = 3;
            code = c & 0x07U;
        }
        else
        {
            return false;
        }
        for (size_t k = 1; k <= extra; ++k)
        {
            if (i + k >= size || ((unsigned char)text[i + k] & 0xC0U) != 0x80U)
            {
                return false;
            }
            code = (code << 6U) | ((unsigned char)text[i + k] & 0x3FU);
        }
        if ((extra == 2 && code < 0x800) || (extra == 3 && code < 0x10000)
            || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
        {
            return false;
        }
        i += extra + 1;
    }
    return true;
}
