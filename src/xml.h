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
