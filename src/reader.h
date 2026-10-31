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
#include "xml.h"
#include "zip.h"

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
    /** The bytes of a stream that the reader collected, or null. */
    unsigned char *owned;
    /** Where the zip file is. */
    x3pio_zip_source source;
    /** The members of the container. */
    x3pio_member_index index;
    /** The description of the file. */
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

    /** The member main.xml, open while the points of a text file are read. */
    x3pio_zip_entry *text_entry;
    /** The Datum elements of a text file, delivered as the rows are read. */
    x3pio_xml_datums *datums;
    /** The numbers of a chunk of a text file. */
    double *text_values;

    /** The room for a chunk. */
    unsigned char *bytes;
    double *raw[3];
    double *values[3];
    double *flag_values;
    unsigned char *flags;
};

/**
 * Releases what a reader holds for reading the points of a text file: the
 * parser and the member. It is done when the last point is read.
 *
 * @param reader The reader.
 */
void x3pio_reader_release_text(x3pio_reader *reader);

/**
 * Releases everything a reader holds for an open file, and makes it new again.
 *
 * @param reader The reader.
 */
void x3pio_reader_reset(x3pio_reader *reader);

/**
 * Makes the room for the conversion of a chunk. It is made when a file is
 * opened, so that reading allocates nothing.
 *
 * @param reader The reader, with a document.
 */
x3pio_status x3pio_reader_prepare(x3pio_reader *reader);

#endif /* X3PIO_SRC_READER_H */
