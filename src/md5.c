/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief MD5 (RFC 1321).
 */

#include "md5.h"
#include "byte_order.h"
#include "text.h"

/* A rotation by a count that is a constant at every call, so that it is one
 * instruction of the processor. */
static inline uint32_t rotate_left(uint32_t x, unsigned n)
{
    return (x << n) | (x >> (32U - n));
}

/* The functions of the four rounds of RFC 1321. */
static inline uint32_t round1(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & y) | (~x & z);
}

static inline uint32_t round2(uint32_t x, uint32_t y, uint32_t z)
{
    return (x & z) | (y & ~z);
}

static inline uint32_t round3(uint32_t x, uint32_t y, uint32_t z)
{
    return x ^ y ^ z;
}

static inline uint32_t round4(uint32_t x, uint32_t y, uint32_t z)
{
    return y ^ (x | ~z);
}

/*
 * One step: b + rotate_left(a + f(b, c, d) + word + constant, shift), which is
 * the new value of a. The constants of RFC 1321 are floor(2^32 * abs(sin(i +
 * 1))). The steps are written out so that the word, the shift and the constant
 * of each are constants of the program: a loop over a table needs a branch for
 * the round, a load for the shift and the constant, a remainder for the word
 * and four moves of the registers for every step, and is a third slower.
 */
static inline uint32_t step(uint32_t a, uint32_t f, uint32_t b, uint32_t word,
                            uint32_t constant, unsigned shift)
{
    return b + rotate_left(a + f + word + constant, shift);
}

static inline void process_block(uint32_t state[4], const unsigned char *block)
{
    uint32_t m[16];
    for (size_t i = 0; i < 16; ++i)
    {
        m[i] = x3pio_load_u32(block + 4 * i);
    }
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];

    a = step(a, round1(b, c, d), b, m[0], 0xd76aa478U, 7);
    d = step(d, round1(a, b, c), a, m[1], 0xe8c7b756U, 12);
    c = step(c, round1(d, a, b), d, m[2], 0x242070dbU, 17);
    b = step(b, round1(c, d, a), c, m[3], 0xc1bdceeeU, 22);
    a = step(a, round1(b, c, d), b, m[4], 0xf57c0fafU, 7);
    d = step(d, round1(a, b, c), a, m[5], 0x4787c62aU, 12);
    c = step(c, round1(d, a, b), d, m[6], 0xa8304613U, 17);
    b = step(b, round1(c, d, a), c, m[7], 0xfd469501U, 22);
    a = step(a, round1(b, c, d), b, m[8], 0x698098d8U, 7);
    d = step(d, round1(a, b, c), a, m[9], 0x8b44f7afU, 12);
    c = step(c, round1(d, a, b), d, m[10], 0xffff5bb1U, 17);
    b = step(b, round1(c, d, a), c, m[11], 0x895cd7beU, 22);
    a = step(a, round1(b, c, d), b, m[12], 0x6b901122U, 7);
    d = step(d, round1(a, b, c), a, m[13], 0xfd987193U, 12);
    c = step(c, round1(d, a, b), d, m[14], 0xa679438eU, 17);
    b = step(b, round1(c, d, a), c, m[15], 0x49b40821U, 22);

    a = step(a, round2(b, c, d), b, m[1], 0xf61e2562U, 5);
    d = step(d, round2(a, b, c), a, m[6], 0xc040b340U, 9);
    c = step(c, round2(d, a, b), d, m[11], 0x265e5a51U, 14);
    b = step(b, round2(c, d, a), c, m[0], 0xe9b6c7aaU, 20);
    a = step(a, round2(b, c, d), b, m[5], 0xd62f105dU, 5);
    d = step(d, round2(a, b, c), a, m[10], 0x02441453U, 9);
    c = step(c, round2(d, a, b), d, m[15], 0xd8a1e681U, 14);
    b = step(b, round2(c, d, a), c, m[4], 0xe7d3fbc8U, 20);
    a = step(a, round2(b, c, d), b, m[9], 0x21e1cde6U, 5);
    d = step(d, round2(a, b, c), a, m[14], 0xc33707d6U, 9);
    c = step(c, round2(d, a, b), d, m[3], 0xf4d50d87U, 14);
    b = step(b, round2(c, d, a), c, m[8], 0x455a14edU, 20);
    a = step(a, round2(b, c, d), b, m[13], 0xa9e3e905U, 5);
    d = step(d, round2(a, b, c), a, m[2], 0xfcefa3f8U, 9);
    c = step(c, round2(d, a, b), d, m[7], 0x676f02d9U, 14);
    b = step(b, round2(c, d, a), c, m[12], 0x8d2a4c8aU, 20);

    a = step(a, round3(b, c, d), b, m[5], 0xfffa3942U, 4);
    d = step(d, round3(a, b, c), a, m[8], 0x8771f681U, 11);
    c = step(c, round3(d, a, b), d, m[11], 0x6d9d6122U, 16);
    b = step(b, round3(c, d, a), c, m[14], 0xfde5380cU, 23);
    a = step(a, round3(b, c, d), b, m[1], 0xa4beea44U, 4);
    d = step(d, round3(a, b, c), a, m[4], 0x4bdecfa9U, 11);
    c = step(c, round3(d, a, b), d, m[7], 0xf6bb4b60U, 16);
    b = step(b, round3(c, d, a), c, m[10], 0xbebfbc70U, 23);
    a = step(a, round3(b, c, d), b, m[13], 0x289b7ec6U, 4);
    d = step(d, round3(a, b, c), a, m[0], 0xeaa127faU, 11);
    c = step(c, round3(d, a, b), d, m[3], 0xd4ef3085U, 16);
    b = step(b, round3(c, d, a), c, m[6], 0x04881d05U, 23);
    a = step(a, round3(b, c, d), b, m[9], 0xd9d4d039U, 4);
    d = step(d, round3(a, b, c), a, m[12], 0xe6db99e5U, 11);
    c = step(c, round3(d, a, b), d, m[15], 0x1fa27cf8U, 16);
    b = step(b, round3(c, d, a), c, m[2], 0xc4ac5665U, 23);

    a = step(a, round4(b, c, d), b, m[0], 0xf4292244U, 6);
    d = step(d, round4(a, b, c), a, m[7], 0x432aff97U, 10);
    c = step(c, round4(d, a, b), d, m[14], 0xab9423a7U, 15);
    b = step(b, round4(c, d, a), c, m[5], 0xfc93a039U, 21);
    a = step(a, round4(b, c, d), b, m[12], 0x655b59c3U, 6);
    d = step(d, round4(a, b, c), a, m[3], 0x8f0ccc92U, 10);
    c = step(c, round4(d, a, b), d, m[10], 0xffeff47dU, 15);
    b = step(b, round4(c, d, a), c, m[1], 0x85845dd1U, 21);
    a = step(a, round4(b, c, d), b, m[8], 0x6fa87e4fU, 6);
    d = step(d, round4(a, b, c), a, m[15], 0xfe2ce6e0U, 10);
    c = step(c, round4(d, a, b), d, m[6], 0xa3014314U, 15);
    b = step(b, round4(c, d, a), c, m[13], 0x4e0811a1U, 21);
    a = step(a, round4(b, c, d), b, m[4], 0xf7537e82U, 6);
    d = step(d, round4(a, b, c), a, m[11], 0xbd3af235U, 10);
    c = step(c, round4(d, a, b), d, m[2], 0x2ad7d2bbU, 15);
    b = step(b, round4(c, d, a), c, m[9], 0xeb86d391U, 21);

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

void x3pio_md5_init(x3pio_md5 *md5)
{
    *md5 = (x3pio_md5){
        {0x67452301U, 0xefcdab89U, 0x98badcfeU, 0x10325476U}, {0}, 0};
}

void x3pio_md5_update(x3pio_md5 *md5, const void *data, size_t size)
{
    const unsigned char *bytes = data;
    size_t used = (size_t)(md5->length % 64U);
    md5->length += size;
    if (used != 0)
    {
        const size_t take = size < 64 - used ? size : 64 - used;
        memcpy(md5->block + used, bytes, take);
        bytes += take;
        size -= take;
        used += take;
        if (used < 64)
        {
            return;
        }
        process_block(md5->state, md5->block);
    }
    while (size >= 64)
    {
        process_block(md5->state, bytes);
        bytes += 64;
        size -= 64;
    }
    if (size != 0)
    {
        memcpy(md5->block, bytes, size);
    }
}

void x3pio_md5_hex(const x3pio_md5 *md5, char out[X3PIO_MD5_HEX_SIZE])
{
    x3pio_md5 copy = *md5;
    const uint64_t bits = md5->length * 8U;
    const unsigned char pad_start = 0x80;
    x3pio_md5_update(&copy, &pad_start, 1);
    const unsigned char zero = 0;
    while (copy.length % 64U != 56U)
    {
        x3pio_md5_update(&copy, &zero, 1);
    }
    unsigned char tail[8];
    for (size_t i = 0; i < 8; ++i)
    {
        tail[i] = (unsigned char)((bits >> (8U * i)) & 0xFFU);
    }
    x3pio_md5_update(&copy, tail, sizeof tail);

    static const char digits[] = "0123456789abcdef";
    size_t used = 0;
    for (size_t k = 0; k < 4; ++k)
    {
        for (unsigned i = 0; i < 4; ++i)
        {
            const unsigned byte = (copy.state[k] >> (8U * i)) & 0xFFU;
            out[used++] = digits[byte >> 4U];
            out[used++] = digits[byte & 0xFU];
        }
    }
    out[used] = '\0';
}

void x3pio_md5_of(const void *data, size_t size, char out[X3PIO_MD5_HEX_SIZE])
{
    x3pio_md5 md5;
    x3pio_md5_init(&md5);
    x3pio_md5_update(&md5, data, size);
    x3pio_md5_hex(&md5, out);
}

bool x3pio_md5_hex_equal(const char *stated,
                         const char computed[X3PIO_MD5_HEX_SIZE])
{
    if (strlen(stated) != 32)
    {
        return false;
    }
    for (size_t i = 0; i < 32; ++i)
    {
        if (x3pio_to_lower(stated[i]) != computed[i])
        {
            return false;
        }
    }
    return true;
}
