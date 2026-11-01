/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The metadata of a document: who created the data, when, and with what.
 */

#include "allocator.h"
#include "document.h"
#include "text.h"
#include "timestamp.h"

/* ---- metadata ----------------------------------------------------------- */

bool x3pio_document_has_metadata(const x3pio_document *document)
{
    return document != NULL && document->has_metadata;
}

void x3pio_document_clear_metadata(x3pio_document *document)
{
    if (document == NULL)
    {
        return;
    }
    x3pio_document_release_text(document, &document->creator);
    x3pio_document_release_text(document, &document->manufacturer);
    x3pio_document_release_text(document, &document->model);
    x3pio_document_release_text(document, &document->serial);
    x3pio_document_release_text(document, &document->version);
    x3pio_document_release_text(document, &document->probing_identification);
    x3pio_document_release_text(document, &document->comment);
    document->has_metadata = false;
    document->has_date = false;
    document->has_calibration = false;
    document->probing = X3PIO_PROBING_UNKNOWN;
}

/* Sets one text of the metadata and creates the metadata in the document. */
static x3pio_status set_text(x3pio_document *document, char **field,
                             const char *text)
{
    if (document == NULL || text == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (!x3pio_document_assign(document, field, text, strlen(text)))
    {
        return X3PIO_ERR_MEMORY;
    }
    document->has_metadata = true;
    return X3PIO_OK;
}

static const char *get_text(const x3pio_document *document, char *const *field)
{
    (void)document;
    return *field != NULL ? *field : "";
}

#define TEXT_ACCESSORS(name, member)                                           \
    const char *x3pio_document_##name(const x3pio_document *document)          \
    {                                                                          \
        return document != NULL ? get_text(document, &document->member) : "";  \
    }                                                                          \
    x3pio_status x3pio_document_set_##name(x3pio_document *document,           \
                                           const char *(name))                 \
    {                                                                          \
        return set_text(document, document != NULL ? &document->member : NULL, \
                        (name));                                               \
    }

TEXT_ACCESSORS(creator, creator)
TEXT_ACCESSORS(manufacturer, manufacturer)
TEXT_ACCESSORS(model, model)
TEXT_ACCESSORS(serial, serial)
TEXT_ACCESSORS(version, version)
TEXT_ACCESSORS(comment, comment)

const char *
x3pio_document_probing_identification(const x3pio_document *document)
{
    return document != NULL
               ? get_text(document, &document->probing_identification)
               : "";
}

x3pio_status
x3pio_document_set_probing_identification(x3pio_document *document,
                                          const char *identification)
{
    return set_text(document,
                    document != NULL ? &document->probing_identification : NULL,
                    identification);
}

bool x3pio_document_date(const x3pio_document *document, x3pio_timestamp *date)
{
    if (document == NULL || !document->has_date)
    {
        return false;
    }
    if (date != NULL)
    {
        *date = document->date;
    }
    return true;
}

x3pio_status x3pio_document_set_date(x3pio_document *document,
                                     const x3pio_timestamp *date)
{
    if (document == NULL || (date != NULL && !x3pio_timestamp_valid(date)))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    document->has_date = date != NULL;
    if (date != NULL)
    {
        document->date = *date;
    }
    document->has_metadata = true;
    return X3PIO_OK;
}

bool x3pio_document_calibration_date(const x3pio_document *document,
                                     x3pio_timestamp *date)
{
    if (document == NULL || !document->has_calibration)
    {
        return false;
    }
    if (date != NULL)
    {
        *date = document->calibration;
    }
    return true;
}

x3pio_status x3pio_document_set_calibration_date(x3pio_document *document,
                                                 const x3pio_timestamp *date)
{
    if (document == NULL || (date != NULL && !x3pio_timestamp_valid(date)))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    document->has_calibration = date != NULL;
    if (date != NULL)
    {
        document->calibration = *date;
    }
    document->has_metadata = true;
    return X3PIO_OK;
}

x3pio_probing x3pio_document_probing_type(const x3pio_document *document)
{
    return document != NULL ? document->probing : X3PIO_PROBING_UNKNOWN;
}

x3pio_status x3pio_document_set_probing_type(x3pio_document *document,
                                             x3pio_probing type)
{
    if (document == NULL
        || (type != X3PIO_PROBING_UNKNOWN && type != X3PIO_PROBING_CONTACTING
            && type != X3PIO_PROBING_NON_CONTACTING
            && type != X3PIO_PROBING_SOFTWARE))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    document->probing = type;
    document->has_metadata = true;
    return X3PIO_OK;
}

x3pio_status x3pio_document_set_software_metadata(x3pio_document *document,
                                                  const char *name,
                                                  const char *version)
{
    if (document == NULL || name == NULL || version == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    x3pio_timestamp now;
    const x3pio_status clock = x3pio_timestamp_now(&now);
    if (clock != X3PIO_OK)
    {
        return clock;
    }
    x3pio_document_clear_metadata(document);
    x3pio_status status = x3pio_document_set_date(document, &now);
    if (status == X3PIO_OK)
    {
        status = x3pio_document_set_model(document, name);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_document_set_version(document, version);
    }
    if (status == X3PIO_OK)
    {
        status =
            x3pio_document_set_probing_type(document, X3PIO_PROBING_SOFTWARE);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_document_set_probing_identification(document, name);
    }
    if (status != X3PIO_OK)
    {
        x3pio_document_clear_metadata(document);
    }
    return status;
}
