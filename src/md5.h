/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief MD5 (RFC 1321), which the format uses for integrity and never for
 * security.
 */

#ifndef X3PIO_SRC_MD5_H
#define X3PIO_SRC_MD5_H

#include "common.h"

/** The size of the hexadecimal digest with its terminator. */
#define X3PIO_MD5_HEX_SIZE 33

/** The state of an incremental MD5. */
typedef struct x3pio_md5
{
    /** The four words of the state. */
    uint32_t state[4];
    /** The bytes of the block that is not complete. */
    unsigned char block[64];
    /** The number of bytes added so far. */
    uint64_t length;
} x3pio_md5;

/**
 * Starts a digest.
 *
 * @param[out] md5 The state.
 */
void x3pio_md5_init(x3pio_md5 *md5);

/**
 * Adds data to the digest.
 *
 * @param[in,out] md5 The state.
 * @param data The bytes.
 * @param size The number of bytes.
 */
void x3pio_md5_update(x3pio_md5 *md5, const void *data, size_t size);

/**
 * Gives the digest of all data added so far as 32 lowercase hexadecimal
 * characters. The state can be used further.
 *
 * @param[in] md5 The state.
 * @param[out] out Receives the digest with a terminator.
 */
void x3pio_md5_hex(const x3pio_md5 *md5, char out[X3PIO_MD5_HEX_SIZE]);

/**
 * Compares a digest that a file states, in upper or lower case, with one that
 * x3pio_md5_hex() computed.
 *
 * @param[in] stated The stated digest, null-terminated.
 * @param[in] computed The computed digest in lower case.
 * @return True if the stated digest has 32 characters and is the same.
 */
bool x3pio_md5_hex_equal(const char *stated,
                         const char computed[X3PIO_MD5_HEX_SIZE]);

/**
 * Gives the digest of a block of memory.
 *
 * @param data The bytes.
 * @param size The number of bytes.
 * @param[out] out Receives the digest with a terminator.
 */
void x3pio_md5_of(const void *data, size_t size, char out[X3PIO_MD5_HEX_SIZE]);

#endif /* X3PIO_SRC_MD5_H */
