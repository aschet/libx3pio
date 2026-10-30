/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Characters, numbers and strings, independent of the locale.
 */

#ifndef X3PIO_SRC_TEXT_H
#define X3PIO_SRC_TEXT_H

#include "allocator.h"
#include "common.h"

/** The size of the longest text of a number the library writes. */
#define X3PIO_NUMBER_CAPACITY 64

/** Tells whether a character is a decimal digit. */
static inline bool x3pio_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/** Tells whether a character is a letter of the alphabet. */
static inline bool x3pio_is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/** Tells whether a character is a hexadecimal digit. */
static inline bool x3pio_is_xdigit(char c)
{
    return x3pio_is_digit(c) || (c >= 'a' && c <= 'f')
           || (c >= 'A' && c <= 'F');
}

/** Tells whether a character is white space of XML. */
static inline bool x3pio_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

/** Converts a letter to lower case. */
static inline char x3pio_to_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/**
 * Removes the white space around a text.
 *
 * @param[in,out] text The start of the text.
 * @param[in,out] size The length of the text.
 */
void x3pio_trim(const char **text, size_t *size);

/**
 * Compares a text with a null-terminated string, ignoring the case of ASCII
 * letters.
 *
 * @param text The text.
 * @param size The length of the text.
 * @param other The string, in lower case.
 */
bool x3pio_equals_nocase(const char *text, size_t size, const char *other);

/**
 * Makes a copy of a text, terminated by a null character.
 *
 * @param allocator The allocator of the copy. Release the copy with it.
 * @param text The text.
 * @param size The length of the text.
 * @return The copy, or null if there is no memory.
 */
char *x3pio_copy_text(const x3pio_allocator *allocator, const char *text,
                      size_t size);

/**
 * Makes a copy of a null-terminated string.
 *
 * @param allocator The allocator of the copy. Release the copy with it.
 * @param text The string.
 * @return The copy, or null if there is no memory.
 */
char *x3pio_strdup(const x3pio_allocator *allocator, const char *text);

/**
 * The decimal point of the locale.
 *
 * It is measured with a print and not read from a table that the C library
 * shares between threads.
 */
typedef struct x3pio_decimal_point
{
    /** The characters, terminated by a null character. */
    char text[8];
} x3pio_decimal_point;

/** Measures the decimal point that the locale has now. */
x3pio_decimal_point x3pio_decimal_point_now(void);

/**
 * Parses a number, whatever the locale. It reads what real files contain: white
 * space around the number, a leading plus, "2.e-06", and `inf`, `infinity` and
 * `nan` in any case.
 *
 * @param decimal_point The decimal point of the locale.
 * @param text The text.
 * @param size The length of the text.
 * @param[out] out Receives the number.
 * @return False if the text is not a number.
 */
bool x3pio_parse_number(const x3pio_decimal_point *decimal_point,
                        const char *text, size_t size, double *out);

/**
 * Formats a number so that it reads back as exactly the same double, with the
 * point as the separator, whatever the locale. Infinity and NaN are `inf`,
 * `-inf` and `nan`.
 *
 * @param decimal_point The decimal point of the locale.
 * @param value The number.
 * @param[out] out Receives the text with a terminator.
 * @param capacity The size of `out`, at least X3PIO_NUMBER_CAPACITY.
 * @return The length of the text.
 */
size_t x3pio_format_number(const x3pio_decimal_point *decimal_point,
                           double value, char *out, size_t capacity);

/**
 * Tells whether a text is well-formed UTF-8: no overlong forms, no surrogates
 * and nothing above U+10FFFF.
 *
 * @param text The text.
 * @param size The length of the text.
 */
bool x3pio_utf8_valid(const char *text, size_t size);

#endif /* X3PIO_SRC_TEXT_H */
