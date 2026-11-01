/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief State of a reader, shared by its source files.
 */

#ifndef X3PIO_SRC_READER_H
#define X3PIO_SRC_READER_H

#include "common.h"
#include "document.h"
#include "md5.h"
#include "members.h"
#include "zip.h"

/** The largest member that is read whole: the validity bits and a checksum. */
#define X3PIO_MAX_SMALL_MEMBER ((size_t)1 << 30U)

/** A file of a vendor in the container. */
typedef struct x3pio_vendor_file
{
    /** The VendorSpecificID. */
    char *id;
    /** The name in the container, or null if the identifier has no file. */
    char *member;
} x3pio_vendor_file;

/** State of a reader. */
struct x3pio_reader
{
    /** The allocator of the reader. */
    x3pio_allocator allocator;
    /** The text of the last failure. */
    const char *message;
    /** Whether checksums are checked. */
    bool verify;
    /** The number of points converted at a time. */
    size_t chunk;
    /** Whether a file is open. */
    bool opened;

    /** The path of the file that was opened by path, or null. */
    char *path;
    /** Where the zip file is. */
    x3pio_zip_source source;
    /** The members of the container. */
    x3pio_member_index index;
    /** The document of the file. */
    x3pio_document *document;
    /** The files of vendors. */
    x3pio_vendor_file *vendors;
    /** The number of files of vendors. */
    size_t vendor_count;

    /** The stored name of the member with the binary points. */
    char *point_name;
    /** The validity bits, or null. */
    unsigned char *valid_bits;
    /** The number of bytes of `valid_bits`. */
    size_t valid_size;
    /** The member with the binary points, open while they are read. */
    x3pio_zip_entry *entry;
    /** The number of rows read. */
    size_t rows_read;
    /** The checksum of the point data read so far. */
    x3pio_md5 hash;
    /** Whether an error in the middle of the points ended the reading. */
    bool failed;

    /** The room for a chunk. */
    unsigned char *bytes;
    double *raw[3];
    double *values[3];
    double *flag_values;
    unsigned char *flags;
};

/**
 * Releases everything a reader holds for an open file, and returns it to the
 * state new.
 *
 * @param reader The reader.
 */
void x3pio_reader_reset(x3pio_reader *reader);

/**
 * Allocates the room for the conversion of a chunk. It is allocated when a file
 * is opened, so that reading allocates nothing.
 *
 * @param reader The reader, with a document.
 */
x3pio_status x3pio_reader_prepare(x3pio_reader *reader);

/**
 * Creates a reader that takes its memory from an allocator, which the tests
 * use to fail an allocation on demand. x3pio_reader_create() uses malloc().
 *
 * @param[in] allocator The allocator, or null for the default. It is copied.
 * @param[out] reader Receives the reader.
 * @retval X3PIO_ERR_ARGUMENT `reader` is null, or the allocator lacks a
 * callback.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
x3pio_status x3pio_reader_create_with(const x3pio_allocator *allocator,
                                      x3pio_reader **reader);

/**
 * Reads main.xml of the opened container into the document of the reader and
 * checks its checksum if the reader verifies.
 *
 * @param[in,out] reader The reader, whose members are listed.
 * @retval X3PIO_ERR_FORMAT main.xml is damaged or not what the standard
 * allows.
 * @retval X3PIO_ERR_CHECKSUM The checksum of main.xml does not match.
 */
x3pio_status x3pio_reader_load_document(x3pio_reader *reader);

/**
 * Finds the binary files of the points and of their validity, and opens the
 * member of the points.
 *
 * @param[in,out] reader The reader, whose document is read.
 */
x3pio_status x3pio_reader_open_binary_files(x3pio_reader *reader);

/**
 * Lists the files of vendors that the document registers and the container
 * holds.
 *
 * @param[in,out] reader The reader, whose document is read.
 */
x3pio_status x3pio_reader_find_vendor_files(x3pio_reader *reader);

#endif /* X3PIO_SRC_READER_H */
