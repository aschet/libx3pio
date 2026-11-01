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
#define ROUND1(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define ROUND2(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define ROUND3(x, y, z) ((x) ^ (y) ^ (z))
#define ROUND4(x, y, z) ((y) ^ ((x) | ~(z)))

/*
 * One step: a = b + rotate_left(a + f(b, c, d) + m[word] + constant, shift).
 * The constants of RFC 1321 are floor(2^32 * abs(sin(i + 1))). The steps are
 * written out so that the word, the shift and the constant of each are
 * constants of the program: a loop over a table needs a branch for the round, a
 * load for the shift and the constant, a remainder for the word and four moves
 * of the registers for every step, and is a third slower.
 */
#define STEP(round, a, b, c, d, word, shift, constant)                         \
    do                                                                         \
    {                                                                          \
        (a) += round((b), (c), (d)) + m[(word)] + (constant);                  \
        (a) = (b) + rotate_left((a), (shift));                                 \
    } while (0)

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

    STEP(ROUND1, a, b, c, d, 0, 7, 0xd76aa478U);
    STEP(ROUND1, d, a, b, c, 1, 12, 0xe8c7b756U);
    STEP(ROUND1, c, d, a, b, 2, 17, 0x242070dbU);
    STEP(ROUND1, b, c, d, a, 3, 22, 0xc1bdceeeU);
    STEP(ROUND1, a, b, c, d, 4, 7, 0xf57c0fafU);
    STEP(ROUND1, d, a, b, c, 5, 12, 0x4787c62aU);
    STEP(ROUND1, c, d, a, b, 6, 17, 0xa8304613U);
    STEP(ROUND1, b, c, d, a, 7, 22, 0xfd469501U);
    STEP(ROUND1, a, b, c, d, 8, 7, 0x698098d8U);
    STEP(ROUND1, d, a, b, c, 9, 12, 0x8b44f7afU);
    STEP(ROUND1, c, d, a, b, 10, 17, 0xffff5bb1U);
    STEP(ROUND1, b, c, d, a, 11, 22, 0x895cd7beU);
    STEP(ROUND1, a, b, c, d, 12, 7, 0x6b901122U);
    STEP(ROUND1, d, a, b, c, 13, 12, 0xfd987193U);
    STEP(ROUND1, c, d, a, b, 14, 17, 0xa679438eU);
    STEP(ROUND1, b, c, d, a, 15, 22, 0x49b40821U);

    STEP(ROUND2, a, b, c, d, 1, 5, 0xf61e2562U);
    STEP(ROUND2, d, a, b, c, 6, 9, 0xc040b340U);
    STEP(ROUND2, c, d, a, b, 11, 14, 0x265e5a51U);
    STEP(ROUND2, b, c, d, a, 0, 20, 0xe9b6c7aaU);
    STEP(ROUND2, a, b, c, d, 5, 5, 0xd62f105dU);
    STEP(ROUND2, d, a, b, c, 10, 9, 0x02441453U);
    STEP(ROUND2, c, d, a, b, 15, 14, 0xd8a1e681U);
    STEP(ROUND2, b, c, d, a, 4, 20, 0xe7d3fbc8U);
    STEP(ROUND2, a, b, c, d, 9, 5, 0x21e1cde6U);
    STEP(ROUND2, d, a, b, c, 14, 9, 0xc33707d6U);
    STEP(ROUND2, c, d, a, b, 3, 14, 0xf4d50d87U);
    STEP(ROUND2, b, c, d, a, 8, 20, 0x455a14edU);
    STEP(ROUND2, a, b, c, d, 13, 5, 0xa9e3e905U);
    STEP(ROUND2, d, a, b, c, 2, 9, 0xfcefa3f8U);
    STEP(ROUND2, c, d, a, b, 7, 14, 0x676f02d9U);
    STEP(ROUND2, b, c, d, a, 12, 20, 0x8d2a4c8aU);

    STEP(ROUND3, a, b, c, d, 5, 4, 0xfffa3942U);
    STEP(ROUND3, d, a, b, c, 8, 11, 0x8771f681U);
    STEP(ROUND3, c, d, a, b, 11, 16, 0x6d9d6122U);
    STEP(ROUND3, b, c, d, a, 14, 23, 0xfde5380cU);
    STEP(ROUND3, a, b, c, d, 1, 4, 0xa4beea44U);
    STEP(ROUND3, d, a, b, c, 4, 11, 0x4bdecfa9U);
    STEP(ROUND3, c, d, a, b, 7, 16, 0xf6bb4b60U);
    STEP(ROUND3, b, c, d, a, 10, 23, 0xbebfbc70U);
    STEP(ROUND3, a, b, c, d, 13, 4, 0x289b7ec6U);
    STEP(ROUND3, d, a, b, c, 0, 11, 0xeaa127faU);
    STEP(ROUND3, c, d, a, b, 3, 16, 0xd4ef3085U);
    STEP(ROUND3, b, c, d, a, 6, 23, 0x04881d05U);
    STEP(ROUND3, a, b, c, d, 9, 4, 0xd9d4d039U);
    STEP(ROUND3, d, a, b, c, 12, 11, 0xe6db99e5U);
    STEP(ROUND3, c, d, a, b, 15, 16, 0x1fa27cf8U);
    STEP(ROUND3, b, c, d, a, 2, 23, 0xc4ac5665U);

    STEP(ROUND4, a, b, c, d, 0, 6, 0xf4292244U);
    STEP(ROUND4, d, a, b, c, 7, 10, 0x432aff97U);
    STEP(ROUND4, c, d, a, b, 14, 15, 0xab9423a7U);
    STEP(ROUND4, b, c, d, a, 5, 21, 0xfc93a039U);
    STEP(ROUND4, a, b, c, d, 12, 6, 0x655b59c3U);
    STEP(ROUND4, d, a, b, c, 3, 10, 0x8f0ccc92U);
    STEP(ROUND4, c, d, a, b, 10, 15, 0xffeff47dU);
    STEP(ROUND4, b, c, d, a, 1, 21, 0x85845dd1U);
    STEP(ROUND4, a, b, c, d, 8, 6, 0x6fa87e4fU);
    STEP(ROUND4, d, a, b, c, 15, 10, 0xfe2ce6e0U);
    STEP(ROUND4, c, d, a, b, 6, 15, 0xa3014314U);
    STEP(ROUND4, b, c, d, a, 13, 21, 0x4e0811a1U);
    STEP(ROUND4, a, b, c, d, 4, 6, 0xf7537e82U);
    STEP(ROUND4, d, a, b, c, 11, 10, 0xbd3af235U);
    STEP(ROUND4, c, d, a, b, 2, 15, 0x2ad7d2bbU);
    STEP(ROUND4, b, c, d, a, 9, 21, 0xeb86d391U);

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
