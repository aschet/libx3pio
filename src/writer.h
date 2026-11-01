/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief State of a writer, shared by its source files.
 */

#ifndef X3PIO_SRC_WRITER_H
#define X3PIO_SRC_WRITER_H

#include "buffer.h"
#include "common.h"
#include "document.h"
#include "md5.h"
#include "zip.h"

/** A file of a vendor waiting to be written with the rest. */
typedef struct x3pio_vendor_entry
{
    /** The name of the member. */
    char *name;
    /** The bytes. */
    unsigned char *content;
    /** The number of bytes. */
    size_t size;
    /** The compression. */
    x3pio_compression compression;
} x3pio_vendor_entry;

/** Where the file goes. */
typedef enum x3pio_target
{
    X3PIO_TARGET_NONE,
    X3PIO_TARGET_FILE,
    X3PIO_TARGET_MEMORY
} x3pio_target;

/** State of a writer. */
struct x3pio_writer
{
    /** The allocator of the writer. */
    x3pio_allocator allocator;
    /** The text of the last failure. */
    const char *message;
    /** How the points are stored. */
    x3pio_storage storage;
    /** The compression of the members. */
    x3pio_compression compression;
    /** The number of points converted at a time. */
    size_t chunk;
    /** Whether a file was created. */
    bool created;
    /** Whether the file was ended. */
    bool ended;
    /** Whether an error ended the writing. */
    bool failed;

    /** The document of the file, copied. */
    x3pio_document *document;
    /** The number of points of the file. */
    size_t count;
    /** The number of points written. */
    size_t written;

    /** Where the file goes. */
    x3pio_target target;
    /** The zip file being written. */
    x3pio_zip_writer *zip;
    /** The zip file in memory, for a file in memory. */
    x3pio_buffer memory;
    /** Where the zip file in memory goes: `memory`, or a buffer of the tests.
     */
    x3pio_buffer *sink;

    /** Whether the member with the binary points was started. */
    bool data_started;
    /** The checksum of the binary points. */
    x3pio_md5 hash;
    /** The validity bits, or null while all points are measured. */
    unsigned char *bits;
    /** Whether a point was not measured. */
    bool missing_seen;
    /** The numbers of a file of text. */
    double *text_values;
    /** The validity of the points of a file of text. */
    unsigned char *text_valid;

    /** The identifiers of the vendors. */
    char **vendor_ids;
    /** The number of identifiers. */
    size_t vendor_id_count;
    /** The files of vendors. */
    x3pio_vendor_entry *vendor_files;
    /** The number of files of vendors. */
    size_t vendor_file_count;

    /** The room for a chunk. */
    unsigned char *bytes;
    double *values[3];
    double *flag_values;
    unsigned char *flags;
};

/**
 * Allocates the room for the conversion of a chunk, and for the points of a
 * file of text. It is allocated when a file is created, so that writing
 * allocates nothing.
 *
 * @param writer The writer, with a document.
 */
x3pio_status x3pio_writer_prepare(x3pio_writer *writer);

/**
 * Creates a writer that takes its memory from an allocator, which the tests
 * use to fail an allocation on demand. x3pio_writer_create() uses malloc().
 *
 * @param[in] allocator The allocator, or null for the default. It is copied.
 * @param[out] writer Receives the writer.
 * @retval X3PIO_ERR_ARGUMENT `writer` is null, or the allocator lacks a
 * callback.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
x3pio_status x3pio_writer_create_with(const x3pio_allocator *allocator,
                                      x3pio_writer **writer);

/**
 * Starts a file in memory like x3pio_create_memory(), but the container goes
 * into a buffer of the caller, which the tests use to hold the file while the
 * writer is alive. The buffer is not released with the writer.
 *
 * @param[in,out] writer The writer.
 * @param[in] document The document of the file.
 * @param[in,out] sink The buffer, which is emptied first.
 * @return The result of x3pio_create_memory().
 */
x3pio_status x3pio_create_memory_into(x3pio_writer *writer,
                                      const x3pio_document *document,
                                      x3pio_buffer *sink);

/**
 * Writes the remaining members and completes the zip file: the validity bits,
 * main.xml, the checksum and the files of vendors.
 *
 * @param writer The writer.
 */
x3pio_status x3pio_writer_finish(x3pio_writer *writer);

#endif /* X3PIO_SRC_WRITER_H */
