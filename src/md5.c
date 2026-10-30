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

static const unsigned shifts[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

/* The constants of RFC 1321: floor(2^32 * abs(sin(i + 1))). */
static const uint32_t constants[64] = {
    0xd76aa478U, 0xe8c7b756U, 0x242070dbU, 0xc1bdceeeU, 0xf57c0fafU,
    0x4787c62aU, 0xa8304613U, 0xfd469501U, 0x698098d8U, 0x8b44f7afU,
    0xffff5bb1U, 0x895cd7beU, 0x6b901122U, 0xfd987193U, 0xa679438eU,
    0x49b40821U, 0xf61e2562U, 0xc040b340U, 0x265e5a51U, 0xe9b6c7aaU,
    0xd62f105dU, 0x02441453U, 0xd8a1e681U, 0xe7d3fbc8U, 0x21e1cde6U,
    0xc33707d6U, 0xf4d50d87U, 0x455a14edU, 0xa9e3e905U, 0xfcefa3f8U,
    0x676f02d9U, 0x8d2a4c8aU, 0xfffa3942U, 0x8771f681U, 0x6d9d6122U,
    0xfde5380cU, 0xa4beea44U, 0x4bdecfa9U, 0xf6bb4b60U, 0xbebfbc70U,
    0x289b7ec6U, 0xeaa127faU, 0xd4ef3085U, 0x04881d05U, 0xd9d4d039U,
    0xe6db99e5U, 0x1fa27cf8U, 0xc4ac5665U, 0xf4292244U, 0x432aff97U,
    0xab9423a7U, 0xfc93a039U, 0x655b59c3U, 0x8f0ccc92U, 0xffeff47dU,
    0x85845dd1U, 0x6fa87e4fU, 0xfe2ce6e0U, 0xa3014314U, 0x4e0811a1U,
    0xf7537e82U, 0xbd3af235U, 0x2ad7d2bbU, 0xeb86d391U};

static uint32_t rotate_left(uint32_t x, unsigned n)
{
    return (x << n) | (x >> (32U - n));
}

static void process_block(uint32_t state[4], const unsigned char *block)
{
    uint32_t m[16];
    for (size_t i = 0; i < 16; ++i)
    {
        m[i] = (uint32_t)block[4 * i] | ((uint32_t)block[4 * i + 1] << 8U)
               | ((uint32_t)block[4 * i + 2] << 16U)
               | ((uint32_t)block[4 * i + 3] << 24U);
    }
    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];
    for (size_t i = 0; i < 64; ++i)
    {
        uint32_t f = 0;
        size_t g = 0;
        if (i < 16)
        {
            f = (b & c) | (~b & d);
            g = i;
        }
        else if (i < 32)
        {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) % 16;
        }
        else if (i < 48)
        {
            f = b ^ c ^ d;
            g = (3 * i + 5) % 16;
        }
        else
        {
            f = c ^ (b | ~d);
            g = (7 * i) % 16;
        }
        f = f + a + constants[i] + m[g];
        a = d;
        d = c;
        c = b;
        b = b + rotate_left(f, shifts[i]);
    }
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
