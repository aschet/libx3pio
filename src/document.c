/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The description of a file.
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

x3pio_document *x3pio_document_make(const x3pio_allocator *allocator)
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
    *document = x3pio_document_make(&used);
    return *document != NULL ? X3PIO_OK : X3PIO_ERR_MEMORY;
}

static void release_text(x3pio_document *document, char **field)
{
    x3pio_release(&document->allocator, *field);
    *field = NULL;
}

void x3pio_document_clear_data(x3pio_document *document)
{
    release_text(document, &document->point_path);
    release_text(document, &document->point_md5);
    release_text(document, &document->valid_path);
    release_text(document, &document->valid_md5);
    release_text(document, &document->checksum_file);
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
    release_text(document, &document->creator);
    release_text(document, &document->manufacturer);
    release_text(document, &document->model);
    release_text(document, &document->serial);
    release_text(document, &document->version);
    release_text(document, &document->probing_identification);
    release_text(document, &document->comment);
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
    x3pio_document *copy = x3pio_document_make(&dst->allocator);
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
     * keeps its structure and its allocator. The copy was made with that
     * allocator, so its memory can be released with it. */
    x3pio_document old = *dst;
    *dst = *copy;
    x3pio_release(&dst->allocator, copy);
    release_content(&old);
    return X3PIO_OK;
}

/* ---- checks ----------------------------------------------------------- */

static bool rotation_valid(const x3pio_transformation *p, const char **message)
{
    const double tolerance = 1e-6;
    const double *r = p->rotation;
    for (size_t i = 0; i < 9; ++i)
    {
        if (!isfinite(r[i]) || r[i] < -1.0 - tolerance
            || r[i] > 1.0 + tolerance)
        {
            *message = "The rotation must be finite and within [-1, 1]";
            return false;
        }
    }
    for (size_t i = 0; i < 3; ++i)
    {
        for (size_t j = 0; j < 3; ++j)
        {
            double dot = 0.0;
            for (size_t k = 0; k < 3; ++k)
            {
                dot += r[3 * i + k] * r[3 * j + k];
            }
            if (fabs(dot - (i == j ? 1.0 : 0.0)) > tolerance)
            {
                *message = "The rotation matrix must be orthonormal";
                return false;
            }
        }
    }
    const double det = r[0] * (r[4] * r[8] - r[5] * r[7])
                       - r[1] * (r[3] * r[8] - r[5] * r[6])
                       + r[2] * (r[3] * r[7] - r[4] * r[6]);
    if (fabs(det - 1.0) > tolerance)
    {
        *message = "The rotation matrix must not mirror (determinant 1)";
        return false;
    }
    return true;
}

static bool transformation_valid(const x3pio_transformation *p,
                                 const char **message)
{
    for (size_t i = 0; i < 3; ++i)
    {
        if (!isfinite(p->offset[i]))
        {
            *message = "The offset of an axis must be finite";
            return false;
        }
    }
    return rotation_valid(p, message);
}

/* The increment of an axis: positive and finite; and for the z axis of integers
 * it can be left to the writer. */
static bool increment_valid(const x3pio_axis *axis, bool may_be_auto)
{
    if (isfinite(axis->increment) && axis->increment > 0.0)
    {
        return true;
    }
    return may_be_auto && axis->increment == X3PIO_AUTO_INCREMENT
           && (axis->storage == X3PIO_TYPE_INT16
               || axis->storage == X3PIO_TYPE_INT32);
}

static bool axis_valid(const x3pio_axis *axis, bool may_be_auto)
{
    return (axis->type == X3PIO_AXIS_INCREMENTAL
            || axis->type == X3PIO_AXIS_ABSOLUTE)
           && increment_valid(axis, may_be_auto)
           && (axis->type == X3PIO_AXIS_INCREMENTAL
               || x3pio_type_is_stored(axis->storage));
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

x3pio_status x3pio_document_validate(const x3pio_document *d,
                                     const char **message)
{
    const char *text = "";
    if (message != NULL)
    {
        *message = text;
    }
    if (d == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
#define FAIL(reason)                                                           \
    do                                                                         \
    {                                                                          \
        text = (reason);                                                       \
        if (message != NULL)                                                   \
        {                                                                      \
            *message = text;                                                   \
        }                                                                      \
        return X3PIO_ERR_ARGUMENT;                                             \
    } while (0)
    if (d->axes[2].type != X3PIO_AXIS_ABSOLUTE)
    {
        FAIL("The z axis must be absolute");
    }
    for (size_t k = 0; k < 3; ++k)
    {
        if (!axis_valid(&d->axes[k], k == 2))
        {
            FAIL("The increment of an axis must be finite and positive, and "
                 "an absolute axis needs a type that files store");
        }
    }
    if (!transformation_valid(&d->transformation, &text))
    {
        if (message != NULL)
        {
            *message = text;
        }
        return X3PIO_ERR_ARGUMENT;
    }
    const bool cloud = d->feature == X3PIO_POINT_CLOUD;
    if (cloud != d->dims.list)
    {
        FAIL("A point cloud is stored as a list, a profile or surface as a "
             "matrix");
    }
    if (cloud
        && (d->axes[0].type != X3PIO_AXIS_ABSOLUTE
            || d->axes[1].type != X3PIO_AXIS_ABSOLUTE))
    {
        FAIL("All axes of a point cloud must be absolute");
    }
    if (d->feature == X3PIO_PROFILE && d->dims.size_y != 1)
    {
        FAIL("A profile has a matrix with one row (size_y of 1)");
    }
    size_t count = 0;
    if (!x3pio_multiply(d->dims.size_x, d->dims.size_y, &count)
        || !x3pio_multiply(count, d->dims.size_z, &count))
    {
        FAIL("The dimensions of the points are too large");
    }
    if (count == 0)
    {
        FAIL("The dimensions of the point data must not be zero");
    }
    if (d->has_metadata)
    {
        if (!d->has_date)
        {
            FAIL("The metadata needs a date");
        }
        if (d->probing == X3PIO_PROBING_UNKNOWN)
        {
            FAIL("The metadata needs the type of the probing system");
        }
    }
#undef FAIL
    return X3PIO_OK;
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
        || !axis_valid(axis, coordinate == X3PIO_Z))
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
        || !transformation_valid(transformation, &message))
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

/* ---- ready-made descriptions ------------------------------------------- */

static bool step_valid(double step)
{
    return isfinite(step) && step > 0.0;
}

x3pio_status x3pio_document_set_surface(x3pio_document *document,
                                        size_t columns, size_t rows,
                                        double x_step, double y_step,
                                        x3pio_type z_type, double z_increment,
                                        size_t layers)
{
    if (document == NULL || !step_valid(x_step) || !step_valid(y_step)
        || !x3pio_type_is_stored(z_type)
        || !(
            step_valid(z_increment)
            || (z_increment == X3PIO_AUTO_INCREMENT
                && (z_type == X3PIO_TYPE_INT16 || z_type == X3PIO_TYPE_INT32))))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    document->feature = X3PIO_SURFACE;
    document->axes[0] =
        (x3pio_axis){X3PIO_AXIS_INCREMENTAL, X3PIO_TYPE_DOUBLE, x_step};
    document->axes[1] =
        (x3pio_axis){X3PIO_AXIS_INCREMENTAL, X3PIO_TYPE_DOUBLE, y_step};
    document->axes[2] = (x3pio_axis){X3PIO_AXIS_ABSOLUTE, z_type, z_increment};
    document->dims = (x3pio_dimensions){columns, rows, layers, false};
    return X3PIO_OK;
}

x3pio_status x3pio_document_set_profile(x3pio_document *document, size_t points,
                                        double x_step, x3pio_type z_type,
                                        double z_increment, size_t layers)
{
    const x3pio_status status = x3pio_document_set_surface(
        document, points, 1, x_step, x_step, z_type, z_increment, layers);
    if (status == X3PIO_OK)
    {
        document->feature = X3PIO_PROFILE;
    }
    return status;
}

x3pio_status x3pio_document_set_point_cloud(x3pio_document *document,
                                            size_t count, x3pio_type type,
                                            double increment)
{
    if (document == NULL || !x3pio_type_is_stored(type)
        || !step_valid(increment))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    document->feature = X3PIO_POINT_CLOUD;
    for (size_t k = 0; k < 3; ++k)
    {
        document->axes[k] = (x3pio_axis){X3PIO_AXIS_ABSOLUTE, type, increment};
    }
    document->dims = (x3pio_dimensions){count, 1, 1, true};
    return X3PIO_OK;
}

x3pio_status x3pio_document_set_irregular_surface(x3pio_document *document,
                                                  size_t columns, size_t rows,
                                                  x3pio_type type,
                                                  double increment,
                                                  size_t layers)
{
    const x3pio_status status =
        x3pio_document_set_point_cloud(document, 0, type, increment);
    if (status == X3PIO_OK)
    {
        document->feature = X3PIO_SURFACE;
        document->dims = (x3pio_dimensions){columns, rows, layers, false};
    }
    return status;
}

x3pio_status x3pio_document_set_irregular_profile(x3pio_document *document,
                                                  size_t points,
                                                  x3pio_type type,
                                                  double increment,
                                                  size_t layers)
{
    if (!step_valid(increment))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    const x3pio_status status = x3pio_document_set_profile(
        document, points, 1.0, type, increment, layers);
    if (status == X3PIO_OK)
    {
        document->axes[X3PIO_X] =
            (x3pio_axis){X3PIO_AXIS_ABSOLUTE, type, increment};
    }
    return status;
}

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
    release_text(document, &document->creator);
    release_text(document, &document->manufacturer);
    release_text(document, &document->model);
    release_text(document, &document->serial);
    release_text(document, &document->version);
    release_text(document, &document->probing_identification);
    release_text(document, &document->comment);
    document->has_metadata = false;
    document->has_date = false;
    document->has_calibration = false;
    document->probing = X3PIO_PROBING_UNKNOWN;
}

/* Sets one text of the metadata and makes the document carry metadata. */
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
