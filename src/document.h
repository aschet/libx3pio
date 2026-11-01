/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The structure behind a document, which the readers and the writers of
 * the files fill and use.
 */

#ifndef X3PIO_SRC_DOCUMENT_H
#define X3PIO_SRC_DOCUMENT_H

#include "common.h"

/** The state of a document. */
struct x3pio_document
{
    /** The allocator of the document. */
    x3pio_allocator allocator;
    /** The revision of the standard. */
    x3pio_revision revision;
    /** The class of the data. */
    x3pio_feature feature;
    /** The axes x, y and z. */
    x3pio_axis axes[3];
    /** Where the view lies in the global system. */
    x3pio_transformation transformation;
    /** The dimensions of the points. */
    x3pio_dimensions dims;

    /** Whether the document has Record2. */
    bool has_metadata;
    /** Whether `date` is set. */
    bool has_date;
    /** The date of the data set. */
    x3pio_timestamp date;
    /** Whether `calibration` is set. */
    bool has_calibration;
    /** The date of the last calibration. */
    x3pio_timestamp calibration;
    /** The kind of the probing system. */
    x3pio_probing probing;
    /** The texts of the metadata, or null. */
    char *creator;
    char *manufacturer;
    char *model;
    char *serial;
    char *version;
    char *probing_identification;
    char *comment;

    /** The identifiers of the vendors. */
    char **vendor_ids;
    /** The number of identifiers. */
    size_t vendor_count;

    /** Whether the points are in a binary file (DataLink). */
    bool binary;
    /** The links of the binary files, and their checksums. */
    char *point_path;
    char *point_md5;
    char *valid_path;
    char *valid_md5;
    /** The name of the checksum file of Record4. */
    char *checksum_file;

    /**
     * The points of a file of text: the stored values of the absolute axes of
     * every point, in the order of the axes. Null for a binary file, and for a
     * document that a program built.
     */
    double *text_values;
    /** The number of values of `text_values`. */
    size_t text_value_count;
    /** One byte per point, 1 for a measured point. Null if all are. */
    unsigned char *text_valid;
};

/**
 * Makes a document with the defaults, as x3pio_document_create() does, with an
 * allocator that has been taken.
 *
 * @param allocator The allocator, which is copied.
 * @return The document, or null if there is no memory.
 */
x3pio_document *x3pio_document_make(const x3pio_allocator *allocator);

/**
 * Creates a document that takes its memory from an allocator, which the tests
 * use to fail an allocation on demand. x3pio_document_create() uses malloc().
 *
 * @param[in] allocator The allocator, or null for the default. It is copied.
 * @param[out] document Receives the document.
 * @retval X3PIO_ERR_ARGUMENT `document` is null, or the allocator lacks a
 * callback.
 * @retval X3PIO_ERR_MEMORY The allocation failed.
 */
x3pio_status x3pio_document_create_with(const x3pio_allocator *allocator,
                                        x3pio_document **document);

/**
 * Sets a text of a document: the old text is released and the new one copied.
 *
 * @param document The document.
 * @param[in,out] field The member that holds the text.
 * @param text The new text, or null to remove it.
 * @param size The length of the text.
 * @return False if there is no memory. The field is unchanged then.
 */
bool x3pio_document_assign(x3pio_document *document, char **field,
                           const char *text, size_t size);

/**
 * Adds a vendor identifier.
 *
 * @param document The document.
 * @param id The identifier.
 * @param size The length of the identifier.
 * @return False if there is no memory.
 */
bool x3pio_document_add_vendor(x3pio_document *document, const char *id,
                               size_t size);

/**
 * Removes the points and links of a file from a document, and its vendors.
 *
 * @param document The document.
 */
void x3pio_document_clear_data(x3pio_document *document);

/**
 * Returns the number of axes that store values.
 *
 * @param[in] document The document.
 */
size_t x3pio_document_absolute_count(const x3pio_document *document);

/**
 * Returns the size in bytes of one point in the binary data file: the values of
 * the absolute axes, one after the other, with no padding.
 *
 * @param[in] document The document.
 */
size_t x3pio_document_record_size(const x3pio_document *document);

/**
 * Returns the position of the value of an absolute axis in a record of the
 * binary data file.
 *
 * @param[in] document The document.
 * @param coordinate The axis.
 */
size_t x3pio_document_record_offset(const x3pio_document *document,
                                    x3pio_coordinate coordinate);

#endif /* X3PIO_SRC_DOCUMENT_H */
