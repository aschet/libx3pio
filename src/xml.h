/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The XML of main.xml, on top of Expat.
 *
 * No other file of the library includes a header of Expat, so this is the seam
 * to replace it.
 */

#ifndef X3PIO_SRC_XML_H
#define X3PIO_SRC_XML_H

#include "buffer.h"
#include "common.h"
#include "document.h"

/**
 * Supplies the bytes of main.xml.
 *
 * @param user The state of the supplier.
 * @param[out] buffer Receives the bytes.
 * @param capacity The size of the buffer.
 * @param[out] got Receives the number of bytes, 0 at the end.
 * @param[out] message Receives the reason of a failure.
 * @return The status of the supply.
 */
typedef x3pio_status (*x3pio_xml_pull)(void *user, void *buffer,
                                       size_t capacity, size_t *got,
                                       const char **message);

/**
 * Parses main.xml into a document. The reader is tolerant: it matches elements
 * by their local name in any order, ignores what it does not know, and accepts
 * the deviations that real files have.
 *
 * @param allocator The allocator of the document and of the parser.
 * @param pull The supplier of the bytes.
 * @param user The state of the supplier.
 * @param[out] document Receives the document, which the caller releases with
 * x3pio_document_free().
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_FORMAT The text is not XML or not a usable x3p document.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 * @return Otherwise the status of the supplier.
 */
x3pio_status x3pio_xml_parse(const x3pio_allocator *allocator,
                             x3pio_xml_pull pull, void *user,
                             x3pio_document **document, const char **message);

/** The points of a file in text, which are delivered a part at a time. */
typedef struct x3pio_xml_datums x3pio_xml_datums;

/**
 * Starts the delivery of the Datum elements of main.xml, which
 * x3pio_xml_parse() has checked: the parser goes through main.xml again, from
 * its first byte, and only the Datum elements are used.
 *
 * @param allocator The allocator of the parser; it must outlive the result.
 * @param pull The supplier of the bytes, from the start of main.xml.
 * @param user The state of the supplier.
 * @param absolute The number of axes that the file stores, which each Datum
 * holds a number for.
 * @param[out] datums Receives the delivery, which the caller releases with
 * x3pio_xml_datums_free().
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
x3pio_status x3pio_xml_datums_open(const x3pio_allocator *allocator,
                                   x3pio_xml_pull pull, void *user,
                                   size_t absolute, x3pio_xml_datums **datums,
                                   const char **message);

/**
 * Delivers the next Datum elements.
 *
 * @param[in,out] datums The delivery.
 * @param n The number of points.
 * @param[out] values Receives `absolute` numbers for each point, NaN for a
 * point that has an empty Datum.
 * @param[out] valid Receives for each point 1, or 0 for an empty Datum.
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_FORMAT There are fewer Datum elements than asked for, or
 * the text is not what x3pio_xml_parse() checked.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 * @return Otherwise the status of the supplier.
 */
x3pio_status x3pio_xml_datums_read(x3pio_xml_datums *datums, size_t n,
                                   double *values, unsigned char *valid,
                                   const char **message);

/**
 * Releases a delivery. A null pointer is ignored.
 *
 * @param[in,out] datums The delivery.
 */
void x3pio_xml_datums_free(x3pio_xml_datums *datums);

/**
 * Receives the bytes of main.xml that the builder produced.
 *
 * @param user The state of the receiver.
 * @param data The bytes.
 * @param size The number of bytes.
 * @param[out] message Receives the reason of a failure.
 * @return The status of the receiver.
 */
typedef x3pio_status (*x3pio_xml_emit)(void *user, const void *data,
                                       size_t size, const char **message);

/**
 * Builds main.xml for a document. The result is valid against the schema of the
 * revision.
 *
 * @param[in] document The document. Its data link or its text values decide how
 * the points are stored.
 * @param emit The receiver of the bytes.
 * @param user The state of the receiver.
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_ARGUMENT A text of the metadata is not UTF-8 or holds a
 * character that XML cannot hold.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 * @return Otherwise the status of the receiver.
 */
x3pio_status x3pio_xml_build(const x3pio_document *document,
                             x3pio_xml_emit emit, void *user,
                             const char **message);

#endif /* X3PIO_SRC_XML_H */
