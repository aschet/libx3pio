/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The document of a file: its fields, the axes, the transformation, the
 * dimensions, the vendors and the record of the binary file.
 */

#include "document.h"
#include "allocator.h"
#include "text.h"
#include "timestamp.h"
#include <limits.h>
#include <math.h>

static x3pio_axis default_axis(x3pio_axis_type type)
{
    return (x3pio_axis){type, X3PIO_TYPE_DOUBLE, 1.0};
}

x3pio_document *x3pio_document_allocate(const x3pio_allocator *allocator)
{
    x3pio_document *document = x3pio_allocate(allocator, sizeof *document);
    if (document == NULL)
    {
        return NULL;
    }
    *document =
        (x3pio_document){.allocator = *allocator,
                         .revision = X3PIO_REVISION_AMD1,
                         .feature = X3PIO_SURFACE,
                         .transformation = x3pio_transformation_identity(),
                         .dims = {0, 1, 1, false}};
    document->axes[0] = default_axis(X3PIO_AXIS_INCREMENTAL);
    document->axes[1] = default_axis(X3PIO_AXIS_INCREMENTAL);
    document->axes[2] = default_axis(X3PIO_AXIS_ABSOLUTE);
    return document;
}

x3pio_status x3pio_document_create(x3pio_document **document)
{
    return x3pio_document_create_with(NULL, document);
}

x3pio_status x3pio_document_create_with(const x3pio_allocator *allocator,
                                        x3pio_document **document)
{
    if (document == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    *document = NULL;
    x3pio_allocator used;
    if (!x3pio_allocator_take(allocator, &used))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    *document = x3pio_document_allocate(&used);
    return *document != NULL ? X3PIO_OK : X3PIO_ERR_MEMORY;
}

void x3pio_document_release_text(x3pio_document *document, char **field)
{
    x3pio_release(&document->allocator, *field);
    *field = NULL;
}

void x3pio_document_clear_data(x3pio_document *document)
{
    x3pio_document_release_text(document, &document->point_path);
    x3pio_document_release_text(document, &document->point_md5);
    x3pio_document_release_text(document, &document->valid_path);
    x3pio_document_release_text(document, &document->valid_md5);
    x3pio_document_release_text(document, &document->checksum_file);
    x3pio_release(&document->allocator, document->text_values);
    x3pio_release(&document->allocator, document->text_valid);
    document->text_values = NULL;
    document->text_valid = NULL;
    document->text_value_count = 0;
    document->binary = false;
    for (size_t i = 0; i < document->vendor_count; ++i)
    {
        x3pio_release(&document->allocator, document->vendor_ids[i]);
    }
    x3pio_release_strings(&document->allocator, document->vendor_ids);
    document->vendor_ids = NULL;
    document->vendor_count = 0;
}

/* Releases everything a document holds, but not the structure. */
static void release_content(x3pio_document *document)
{
    x3pio_document_clear_data(document);
    x3pio_document_release_text(document, &document->creator);
    x3pio_document_release_text(document, &document->manufacturer);
    x3pio_document_release_text(document, &document->model);
    x3pio_document_release_text(document, &document->serial);
    x3pio_document_release_text(document, &document->version);
    x3pio_document_release_text(document, &document->probing_identification);
    x3pio_document_release_text(document, &document->comment);
}

void x3pio_document_free(x3pio_document *document)
{
    if (document == NULL)
    {
        return;
    }
    release_content(document);
    const x3pio_allocator allocator = document->allocator;
    x3pio_release(&allocator, document);
}

bool x3pio_document_assign(x3pio_document *document, char **field,
                           const char *text, size_t size)
{
    char *copy = NULL;
    if (text != NULL)
    {
        copy = x3pio_copy_text(&document->allocator, text, size);
        if (copy == NULL)
        {
            return false;
        }
    }
    x3pio_release(&document->allocator, *field);
    *field = copy;
    return true;
}

bool x3pio_document_add_vendor(x3pio_document *document, const char *id,
                               size_t size)
{
    char *copy = x3pio_copy_text(&document->allocator, id, size);
    if (copy == NULL)
    {
        return false;
    }
    char **grown = x3pio_allocate_strings(&document->allocator,
                                          document->vendor_count + 1);
    if (grown == NULL)
    {
        x3pio_release(&document->allocator, copy);
        return false;
    }
    x3pio_copy_strings(grown, document->vendor_ids, document->vendor_count);
    x3pio_release_strings(&document->allocator, document->vendor_ids);
    grown[document->vendor_count] = copy;
    document->vendor_ids = grown;
    ++document->vendor_count;
    return true;
}

/* Copies a text member of one document to another, with the allocator of the
 * destination. */
static bool copy_member(x3pio_document *dst, char **field, const char *text)
{
    return x3pio_document_assign(dst, field, text,
                                 text != NULL ? strlen(text) : 0);
}

x3pio_status x3pio_document_copy(x3pio_document *dst, const x3pio_document *src)
{
    if (dst == NULL || src == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (dst == src)
    {
        return X3PIO_OK;
    }
    /* The copy is built in a document of its own first, so that a failure
     * leaves the destination as it was. */
    x3pio_document *copy = x3pio_document_allocate(&dst->allocator);
    if (copy == NULL)
    {
        return X3PIO_ERR_MEMORY;
    }
    copy->revision = src->revision;
    copy->feature = src->feature;
    memcpy(copy->axes, src->axes, sizeof copy->axes);
    copy->transformation = src->transformation;
    copy->dims = src->dims;
    copy->has_metadata = src->has_metadata;
    copy->has_date = src->has_date;
    copy->date = src->date;
    copy->has_calibration = src->has_calibration;
    copy->calibration = src->calibration;
    copy->probing = src->probing;
    bool ok = copy_member(copy, &copy->creator, src->creator)
              && copy_member(copy, &copy->manufacturer, src->manufacturer)
              && copy_member(copy, &copy->model, src->model)
              && copy_member(copy, &copy->serial, src->serial)
              && copy_member(copy, &copy->version, src->version)
              && copy_member(copy, &copy->probing_identification,
                             src->probing_identification)
              && copy_member(copy, &copy->comment, src->comment);
    for (size_t i = 0; ok && i < src->vendor_count; ++i)
    {
        ok = x3pio_document_add_vendor(copy, src->vendor_ids[i],
                                       strlen(src->vendor_ids[i]));
    }
    copy->binary = src->binary;
    ok = ok && copy_member(copy, &copy->point_path, src->point_path)
         && copy_member(copy, &copy->point_md5, src->point_md5)
         && copy_member(copy, &copy->valid_path, src->valid_path)
         && copy_member(copy, &copy->valid_md5, src->valid_md5)
         && copy_member(copy, &copy->checksum_file, src->checksum_file);
    if (ok && src->text_values != NULL)
    {
        copy->text_values = x3pio_allocate_array(
            &copy->allocator, src->text_value_count, sizeof(double));
        ok = copy->text_values != NULL;
        if (ok)
        {
            memcpy(copy->text_values, src->text_values,
                   src->text_value_count * sizeof(double));
            copy->text_value_count = src->text_value_count;
        }
    }
    if (ok && src->text_valid != NULL)
    {
        const size_t count = x3pio_document_point_count(src);
        copy->text_valid = x3pio_allocate(&copy->allocator, count);
        ok = copy->text_valid != NULL;
        if (ok)
        {
            memcpy(copy->text_valid, src->text_valid, count);
        }
    }
    if (!ok)
    {
        x3pio_document_free(copy);
        return X3PIO_ERR_MEMORY;
    }
    /* The content of the copy replaces the content of the destination, which
     * keeps its structure and its allocator. The copy was created with that
     * allocator, so its memory can be released with it. */
    x3pio_document old = *dst;
    *dst = *copy;
    x3pio_release(&dst->allocator, copy);
    release_content(&old);
    return X3PIO_OK;
}

size_t x3pio_document_point_count(const x3pio_document *document)
{
    size_t count = 0;
    if (document == NULL
        || !x3pio_multiply(document->dims.size_x, document->dims.size_y, &count)
        || !x3pio_multiply(count, document->dims.size_z, &count))
    {
        return 0;
    }
    return count;
}

/* ---- revision, feature, axes, transformation, dimensions --------------------
 */

x3pio_revision x3pio_document_revision(const x3pio_document *document)
{
    return document != NULL ? document->revision : X3PIO_REVISION_AMD1;
}

x3pio_status x3pio_document_set_revision(x3pio_document *document,
                                         x3pio_revision revision)
{
    if (document == NULL
        || (revision != X3PIO_REVISION_2017 && revision != X3PIO_REVISION_AMD1))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    document->revision = revision;
    return X3PIO_OK;
}

x3pio_feature x3pio_document_feature(const x3pio_document *document)
{
    return document != NULL ? document->feature : X3PIO_SURFACE;
}

x3pio_status x3pio_document_set_feature(x3pio_document *document,
                                        x3pio_feature feature)
{
    if (document == NULL
        || (feature != X3PIO_PROFILE && feature != X3PIO_SURFACE
            && feature != X3PIO_POINT_CLOUD))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    document->feature = feature;
    return X3PIO_OK;
}

x3pio_axis x3pio_document_axis(const x3pio_document *document,
                               x3pio_coordinate coordinate)
{
    if (document == NULL || (unsigned)coordinate > 2U)
    {
        return default_axis(X3PIO_AXIS_INCREMENTAL);
    }
    return document->axes[coordinate];
}

x3pio_status x3pio_document_get_axis(const x3pio_document *document,
                                     x3pio_coordinate coordinate,
                                     x3pio_axis *axis)
{
    if (document == NULL || axis == NULL || (unsigned)coordinate > 2U)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    *axis = document->axes[coordinate];
    return X3PIO_OK;
}

x3pio_status x3pio_document_set_axis(x3pio_document *document,
                                     x3pio_coordinate coordinate,
                                     const x3pio_axis *axis)
{
    if (document == NULL || axis == NULL || (unsigned)coordinate > 2U
        || !x3pio_axis_valid(axis, coordinate == X3PIO_Z))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    document->axes[coordinate] = *axis;
    if (!x3pio_type_is_stored(axis->storage))
    {
        document->axes[coordinate].storage = X3PIO_TYPE_DOUBLE;
    }
    return X3PIO_OK;
}

x3pio_transformation
x3pio_document_transformation(const x3pio_document *document)
{
    return document != NULL ? document->transformation
                            : x3pio_transformation_identity();
}

x3pio_status
x3pio_document_get_transformation(const x3pio_document *document,
                                  x3pio_transformation *transformation)
{
    if (document == NULL || transformation == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    *transformation = document->transformation;
    return X3PIO_OK;
}

x3pio_status
x3pio_document_set_transformation(x3pio_document *document,
                                  const x3pio_transformation *transformation)
{
    const char *message = "";
    if (document == NULL || transformation == NULL
        || !x3pio_transformation_valid(transformation, &message))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    document->transformation = *transformation;
    return X3PIO_OK;
}

x3pio_dimensions x3pio_document_dimensions(const x3pio_document *document)
{
    return document != NULL ? document->dims
                            : (x3pio_dimensions){0, 1, 1, false};
}

x3pio_status x3pio_document_get_dimensions(const x3pio_document *document,
                                           x3pio_dimensions *dimensions)
{
    if (document == NULL || dimensions == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    *dimensions = document->dims;
    return X3PIO_OK;
}

x3pio_status x3pio_document_set_matrix(x3pio_document *document, size_t size_x,
                                       size_t size_y, size_t size_z)
{
    if (document == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    document->dims = (x3pio_dimensions){size_x, size_y, size_z, false};
    return X3PIO_OK;
}

x3pio_status x3pio_document_set_point_list(x3pio_document *document,
                                           size_t count)
{
    if (document == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    document->dims = (x3pio_dimensions){count, 1, 1, true};
    return X3PIO_OK;
}

/* ---- vendors ------------------------------------------------------------ */

size_t x3pio_document_vendor_count(const x3pio_document *document)
{
    return document != NULL ? document->vendor_count : 0;
}

const char *x3pio_document_vendor_id(const x3pio_document *document,
                                     size_t index)
{
    if (document == NULL || index >= document->vendor_count)
    {
        return NULL;
    }
    return document->vendor_ids[index];
}

/* ---- the layout of a record in the binary file ------------------------- */

size_t x3pio_document_columns(const x3pio_document *document)
{
    if (document == NULL)
    {
        return 0;
    }
    return document->dims.list ? 1 : document->dims.size_x;
}

size_t x3pio_document_rows(const x3pio_document *document)
{
    size_t rows = 0;
    if (document == NULL)
    {
        return 0;
    }
    if (document->dims.list)
    {
        return document->dims.size_x;
    }
    return x3pio_multiply(document->dims.size_y, document->dims.size_z, &rows)
               ? rows
               : 0;
}

bool x3pio_document_has_validity(const x3pio_document *document)
{
    return document != NULL
           && ((document->valid_path != NULL && document->valid_path[0] != '\0')
               || document->text_valid != NULL);
}

size_t x3pio_document_absolute_count(const x3pio_document *document)
{
    size_t count = 0;
    for (size_t k = 0; k < 3; ++k)
    {
        count += document->axes[k].type == X3PIO_AXIS_ABSOLUTE ? 1U : 0U;
    }
    return count;
}

size_t x3pio_document_record_size(const x3pio_document *document)
{
    size_t size = 0;
    for (size_t k = 0; k < 3; ++k)
    {
        if (document->axes[k].type == X3PIO_AXIS_ABSOLUTE)
        {
            size += x3pio_type_size(document->axes[k].storage);
        }
    }
    return size;
}

size_t x3pio_document_record_offset(const x3pio_document *document,
                                    x3pio_coordinate coordinate)
{
    size_t offset = 0;
    for (size_t k = 0; k < (size_t)coordinate && k < 3; ++k)
    {
        if (document->axes[k].type == X3PIO_AXIS_ABSOLUTE)
        {
            offset += x3pio_type_size(document->axes[k].storage);
        }
    }
    return offset;
}
