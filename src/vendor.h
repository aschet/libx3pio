/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Names of the members of an x3p container.
 */

#ifndef X3PIO_SRC_VENDOR_H
#define X3PIO_SRC_VENDOR_H

#include "common.h"

/** The central document of every container. */
#define X3PIO_MAIN_XML "main.xml"
/** The recommended name of the checksum file of main.xml. */
#define X3PIO_CHECKSUM_FILE "md5checksum.hex"
/** The recommended path of the binary coordinate file. */
#define X3PIO_DATA_FILE "bindata/data.bin"
/** The recommended path of the binary validity file. */
#define X3PIO_VALID_FILE "bindata/valid.bin"

/**
 * Normalizes a link of main.xml to a member of the container: backslashes
 * separate, and "." segments and empty segments are dropped.
 *
 * @param allocator The allocator of the result.
 * @param link The link.
 * @param size The length of the link.
 * @param[out] path Receives the path, released with the allocator.
 * @retval X3PIO_ERR_FORMAT The link is empty, absolute, has a URI scheme or a
 * drive letter, or climbs out of the container with "..".
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
x3pio_status x3pio_member_path(const x3pio_allocator *allocator,
                               const char *link, size_t size, char **path);

/**
 * Tells whether a normalized path is a name that the writer always uses itself.
 *
 * @param path The normalized path.
 */
bool x3pio_is_reserved_name(const char *path);

/**
 * The member of the container for the file of a VendorSpecificID in Amd 1: the
 * host name and the path of the URI with every separator replaced by a
 * backslash, so "http://www.vendor.com/my/a.xml" gives
 * `www\vendor\com\my\a.xml`.
 *
 * @param allocator The allocator of the result.
 * @param uri The identifier.
 * @param size The length of the identifier.
 * @param[out] member Receives the member, released with the allocator.
 * @retval X3PIO_ERR_ARGUMENT The identifier has no host name or no path to a
 * file.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
x3pio_status x3pio_vendor_member(const x3pio_allocator *allocator,
                                 const char *uri, size_t size, char **member);

#endif /* X3PIO_SRC_VENDOR_H */
