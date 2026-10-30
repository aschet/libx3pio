/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* MD5 matches the test suite of RFC 1321, however the data is split. */

#include "check.h"
#include "md5.h"

static void check_digest(const char *text, const char *expected)
{
    char digest[X3PIO_MD5_HEX_SIZE];
    x3pio_md5_of(text, strlen(text), digest);
    CHECK_STRING(digest, expected);
}

int main(void)
{
    check_digest("", "d41d8cd98f00b204e9800998ecf8427e");
    check_digest("a", "0cc175b9c0f1b6a831c399e269772661");
    check_digest("abc", "900150983cd24fb0d6963f7d28e17f72");
    check_digest("message digest", "f96b697d7cb7938d525a2f31aaf161d0");
    check_digest("abcdefghijklmnopqrstuvwxyz",
                 "c3fcd3d76192e4007dfb496cca67e13b");
    check_digest(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
        "d174ab98d277d9f5a5611c2c9f419d9f");
    check_digest("1234567890123456789012345678901234567890123456789012345678901"
                 "2345678901234567890",
                 "57edf4a22be3c955ac49da2e2107b67a");

    /* However the data is split. */
    unsigned char data[1000];
    for (size_t i = 0; i < sizeof data; ++i)
    {
        data[i] = (unsigned char)(i * 7 + 3);
    }
    char whole[X3PIO_MD5_HEX_SIZE];
    x3pio_md5_of(data, sizeof data, whole);
    for (size_t step = 1; step <= 130; step += 7)
    {
        x3pio_md5 md5;
        x3pio_md5_init(&md5);
        for (size_t at = 0; at < sizeof data; at += step)
        {
            const size_t n = sizeof data - at < step ? sizeof data - at : step;
            x3pio_md5_update(&md5, data + at, n);
        }
        char split[X3PIO_MD5_HEX_SIZE];
        x3pio_md5_hex(&md5, split);
        CHECK_STRING(split, whole);
        /* The state can be used further. */
        char again[X3PIO_MD5_HEX_SIZE];
        x3pio_md5_hex(&md5, again);
        CHECK_STRING(again, whole);
    }

    /* A million times a. */
    x3pio_md5 md5;
    x3pio_md5_init(&md5);
    unsigned char block[1000];
    memset(block, 'a', sizeof block);
    for (int i = 0; i < 1000; ++i)
    {
        x3pio_md5_update(&md5, block, sizeof block);
    }
    char digest[X3PIO_MD5_HEX_SIZE];
    x3pio_md5_hex(&md5, digest);
    CHECK_STRING(digest, "7707d6ae4e027c70eea2a935c2296f21");
    return finish_tests();
}
