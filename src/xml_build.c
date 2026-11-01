/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Putting the document together from what the scan of main.xml
 * collected.
 */

#include "allocator.h"
#include "text.h"
#include "timestamp.h"
#include "xml_state.h"
#include <limits.h>
#include <math.h>

/* The text of a leaf, without white space around it; empty if it is missing. */
static const char *leaf_text(const state *s, int id, size_t *size)
{
    const char *text = s->leaves[id] != NULL ? s->leaves[id] : "";
    *size = strlen(text);
    x3pio_trim(&text, size);
    return text;
}

static bool have(const state *s, int id)
{
    return s->leaf_seen[id];
}

/* A number from a leaf; the fallback if the leaf is missing or empty. */
static x3pio_status number_of(state *s, int id, double fallback, double *out)
{
    size_t size = 0;
    const char *text = leaf_text(s, id, &size);
    if (size == 0)
    {
        *out = fallback;
        return X3PIO_OK;
    }
    double value = 0.0;
    if (!x3pio_parse_number(&s->point, text, size, &value) || !isfinite(value))
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "A number of main.xml is not valid");
    }
    *out = value;
    return X3PIO_OK;
}

static x3pio_status size_of_leaf(state *s, int id, size_t *out)
{
    size_t size = 0;
    const char *text = leaf_text(s, id, &size);
    double value = 0.0;
    if (!x3pio_parse_number(&s->point, text, size, &value) || !(value >= 0.0)
        || value != floor(value) || value > 9007199254740992.0
        || value > (double)SIZE_MAX)
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "A size of main.xml is not valid");
    }
    *out = (size_t)value;
    return X3PIO_OK;
}

/* The revision that the text of the Revision element names, which is false for
 * a text that names none. The dashes that appear in real files instead of '-',
 * U+2010 to U+2015 and the minus sign U+2212 in UTF-8, count as a hyphen. */
static bool revision_of_text(const char *text, size_t size,
                             x3pio_revision *revision)
{
    static const char amd1[] = "ISO25178-72:2017/DAM1";
    static const char first[] = "ISO5436 - 2000";
    char normalized[sizeof amd1 + 8];
    size_t used = 0;
    for (size_t i = 0; i < size; ++i)
    {
        const unsigned char c0 = (unsigned char)text[i];
        char c = (char)c0;
        if (c0 == 0xE2 && i + 2 < size)
        {
            const unsigned char c1 = (unsigned char)text[i + 1];
            const unsigned char c2 = (unsigned char)text[i + 2];
            if ((c1 == 0x80 && c2 >= 0x90 && c2 <= 0x95)
                || (c1 == 0x88 && c2 == 0x92))
            {
                c = '-';
                i += 2;
            }
        }
        if (used >= sizeof normalized)
        {
            return false;
        }
        normalized[used++] = c;
    }
    /* A missing revision is the first one, as in files of the first
     * implementations. */
    if (used == 0
        || (used == sizeof first - 1 && memcmp(normalized, first, used) == 0))
    {
        *revision = X3PIO_REVISION_2017;
        return true;
    }
    if (used == sizeof amd1 - 1 && memcmp(normalized, amd1, used) == 0)
    {
        *revision = X3PIO_REVISION_AMD1;
        return true;
    }
    return false;
}

static bool parse_code(const char *text, size_t size, const char *codes,
                       size_t *index)
{
    if (size != 1)
    {
        return false;
    }
    const char c =
        (char)((text[0] >= 'a' && text[0] <= 'z') ? text[0] - 'a' + 'A'
                                                  : text[0]);
    const char *found = strchr(codes, c);
    if (found == NULL || c == '\0')
    {
        return false;
    }
    *index = (size_t)(found - codes);
    return true;
}

static x3pio_status build_axis(state *s, size_t k, double *offset)
{
    x3pio_axis *axis = &s->document->axes[k];
    const int base = L_AXIS_BASE + 4 * (int)k;
    size_t size = 0;
    if (!have(s, base + L_AXIS_TYPE))
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "An axis has no AxisType");
    }
    const char *text = leaf_text(s, base + L_AXIS_TYPE, &size);
    size_t index = 0;
    if (!parse_code(text, size, "IA", &index))
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "An axis has an unknown AxisType");
    }
    axis->type = index == 0 ? X3PIO_AXIS_INCREMENTAL : X3PIO_AXIS_ABSOLUTE;
    text = leaf_text(s, base + L_AXIS_DATA, &size);
    axis->storage = X3PIO_TYPE_DOUBLE;
    if (size == 0)
    {
        if (axis->type == X3PIO_AXIS_ABSOLUTE)
        {
            return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                              "An absolute axis has no DataType");
        }
    }
    else if (parse_code(text, size, "ILFD", &index))
    {
        static const x3pio_type types[4] = {X3PIO_TYPE_INT16, X3PIO_TYPE_INT32,
                                            X3PIO_TYPE_FLOAT,
                                            X3PIO_TYPE_DOUBLE};
        axis->storage = types[index];
    }
    else
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "An axis has an unknown DataType");
    }
    x3pio_status status =
        number_of(s, base + L_AXIS_INCREMENT, 1.0, &axis->increment);
    if (status != X3PIO_OK)
    {
        return status;
    }
    if (axis->increment == 0.0)
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "An axis has an Increment of zero");
    }
    return number_of(s, base + L_AXIS_OFFSET, 0.0, offset);
}

static x3pio_status build_rotation(state *s, int base,
                                   x3pio_transformation *place)
{
    for (int i = 0; i < 9; ++i)
    {
        if (!have(s, base + i))
        {
            return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                              "The rotation lacks an element");
        }
        double v = 0.0;
        const x3pio_status status = number_of(s, base + i, 0.0, &v);
        if (status != X3PIO_OK)
        {
            return status;
        }
        /* Elements a hair beyond +-1 come from rounding when writing. */
        if (fabs(v) > 1.0 + 1e-6)
        {
            return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                              "A rotation element lies outside [-1, 1]");
        }
        place->rotation[i] = x3pio_clamp_unit(v);
    }
    return X3PIO_OK;
}

static bool any_of(const state *s, int base, int count)
{
    for (int i = 0; i < count; ++i)
    {
        if (have(s, base + i))
        {
            return true;
        }
    }
    return false;
}

static bool assign(state *s, char **field, int id, bool raw)
{
    size_t size = 0;
    const char *text;
    if (raw)
    {
        text = s->leaves[id] != NULL ? s->leaves[id] : "";
        size = strlen(text);
    }
    else
    {
        text = leaf_text(s, id, &size);
    }
    if (!x3pio_document_assign(s->document, field, text, size))
    {
        x3pio_xml_fail_memory(s);
        return false;
    }
    return true;
}

static x3pio_status build_metadata(state *s)
{
    x3pio_document *d = s->document;
    d->has_metadata = true;
    size_t size = 0;
    const char *text = leaf_text(s, L_DATE, &size);
    char buffer[64];
    /* A date that is no date, such as "N/A", is no date. */
    if (size < sizeof buffer)
    {
        memcpy(buffer, text, size);
        buffer[size] = '\0';
        d->has_date = x3pio_timestamp_parse(buffer, &d->date);
    }
    text = leaf_text(s, L_CALIBRATION, &size);
    if (size < sizeof buffer)
    {
        memcpy(buffer, text, size);
        buffer[size] = '\0';
        d->has_calibration = x3pio_timestamp_parse(buffer, &d->calibration);
    }
    text = leaf_text(s, L_PROBING_TYPE, &size);
    d->probing = X3PIO_PROBING_UNKNOWN;
    if (x3pio_equals_nocase(text, size, "contacting"))
    {
        d->probing = X3PIO_PROBING_CONTACTING;
    }
    else if (x3pio_equals_nocase(text, size, "noncontacting"))
    {
        d->probing = X3PIO_PROBING_NON_CONTACTING;
    }
    else if (x3pio_equals_nocase(text, size, "software"))
    {
        d->probing = X3PIO_PROBING_SOFTWARE;
    }
    if (!assign(s, &d->creator, L_CREATOR, false)
        || !assign(s, &d->manufacturer, L_MANUFACTURER, false)
        || !assign(s, &d->model, L_MODEL, false)
        || !assign(s, &d->serial, L_SERIAL, false)
        || !assign(s, &d->version, L_VERSION, false)
        || !assign(s, &d->probing_identification, L_PROBING_ID, false)
        || !assign(s, &d->comment, L_COMMENT, true))
    {
        return X3PIO_ERR_MEMORY;
    }
    return X3PIO_OK;
}

/* Checks the Datum elements against the axes and moves their numbers into the
 * document. */
static x3pio_status build_datums(state *s)
{
    x3pio_document *d = s->document;
    const size_t absolute = x3pio_document_absolute_count(d);
    const size_t count = x3pio_document_point_count(d);
    if (s->datum_count != count)
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "DataList does not hold a Datum for every point");
    }
    size_t total = 0;
    if (!x3pio_multiply(count, absolute, &total))
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "The dimensions of the points are too large");
    }
    x3pio_coordinate axes[3];
    size_t axis_count = 0;
    for (size_t k = 0; k < 3; ++k)
    {
        if (d->axes[k].type == X3PIO_AXIS_ABSOLUTE)
        {
            axes[axis_count++] = (x3pio_coordinate)k;
        }
    }
    d->text_values = x3pio_allocate_array(&d->allocator, total, sizeof(double));
    d->text_valid = x3pio_allocate(&d->allocator, count);
    if (d->text_values == NULL || d->text_valid == NULL)
    {
        return x3pio_fail(s->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    d->text_value_count = total;
    size_t from = 0;
    bool all_valid = true;
    for (size_t i = 0; i < count; ++i)
    {
        const size_t fields = s->counts[i];
        if (fields == 0)
        {
            for (size_t k = 0; k < absolute; ++k)
            {
                d->text_values[i * absolute + k] = NAN;
            }
            d->text_valid[i] = 0;
            all_valid = false;
            continue;
        }
        if (fields != absolute)
        {
            return x3pio_fail(
                s->message, X3PIO_ERR_FORMAT,
                "A Datum does not hold a number for every axis that is stored");
        }
        for (size_t k = 0; k < absolute; ++k)
        {
            const double value = s->values[from + k];
            const x3pio_type type = d->axes[axes[k]].storage;
            if (type == X3PIO_TYPE_INT16 || type == X3PIO_TYPE_INT32)
            {
                const double limit =
                    type == X3PIO_TYPE_INT16 ? 32768.0 : 2147483648.0;
                if (value != floor(value) || value < -limit || value >= limit)
                {
                    return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                                      "A Datum holds a number that is no value "
                                      "of an integer axis");
                }
            }
            d->text_values[i * absolute + k] = value;
        }
        from += fields;
        d->text_valid[i] = 1;
    }
    if (all_valid)
    {
        x3pio_release(&d->allocator, d->text_valid);
        d->text_valid = NULL;
    }
    return X3PIO_OK;
}

x3pio_status x3pio_xml_finish(state *s)
{
    x3pio_document *d = s->document;
    const char **m = s->message;
    if (!s->root_seen)
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT,
                          "main.xml has no ISO5436_2 element");
    }
    if (!s->container_seen[E_RECORD1])
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT, "main.xml has no Record1");
    }
    if (!s->container_seen[E_AXES])
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT, "Record1 has no Axes");
    }
    double offsets[3] = {0, 0, 0};
    for (size_t k = 0; k < 3; ++k)
    {
        if (!s->container_seen[E_AXIS_X + k])
        {
            return x3pio_fail(m, X3PIO_ERR_FORMAT,
                              "Axes lacks one of CX, CY and CZ");
        }
        const x3pio_status status = build_axis(s, k, &offsets[k]);
        if (status != X3PIO_OK)
        {
            return status;
        }
    }
    memcpy(d->transformation.offset, offsets, sizeof offsets);
    if (d->axes[2].type != X3PIO_AXIS_ABSOLUTE)
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT, "The z axis must be absolute");
    }
    x3pio_status status = X3PIO_OK;
    if (any_of(s, L_ROTATION_AXES, 9) || s->container_seen[E_ROTATION_AXES])
    {
        status = build_rotation(s, L_ROTATION_AXES, &d->transformation);
    }
    else if (any_of(s, L_ROTATION_RECORD, 9)
             || s->container_seen[E_ROTATION_RECORD])
    {
        status = build_rotation(s, L_ROTATION_RECORD, &d->transformation);
    }
    if (status != X3PIO_OK)
    {
        return status;
    }
    if (!have(s, L_FEATURE))
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT, "Record1 has no FeatureType");
    }
    size_t size = 0;
    const char *text = leaf_text(s, L_FEATURE, &size);
    x3pio_feature feature = X3PIO_SURFACE;
    if (x3pio_equals_nocase(text, size, "prf"))
    {
        feature = X3PIO_PROFILE;
    }
    else if (x3pio_equals_nocase(text, size, "sur"))
    {
        feature = X3PIO_SURFACE;
    }
    else if (x3pio_equals_nocase(text, size, "pcl"))
    {
        feature = X3PIO_POINT_CLOUD;
    }
    else
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT,
                          "Record1 has an unknown FeatureType");
    }
    if (s->container_seen[E_RECORD2])
    {
        status = build_metadata(s);
        if (status != X3PIO_OK)
        {
            return status;
        }
    }
    if (!s->container_seen[E_RECORD3])
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT, "main.xml has no Record3");
    }
    const bool matrix = s->container_seen[E_MATRIX];
    const bool list = have(s, L_LIST);
    if (matrix == list)
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT,
                          "Record3 needs exactly one of MatrixDimension and "
                          "ListDimension");
    }
    if (matrix)
    {
        size_t u = 0;
        size_t v = 0;
        size_t w = 0;
        if (!have(s, L_SIZE_X) || !have(s, L_SIZE_Y) || !have(s, L_SIZE_Z))
        {
            return x3pio_fail(m, X3PIO_ERR_FORMAT,
                              "MatrixDimension needs SizeX, SizeY and SizeZ");
        }
        status = size_of_leaf(s, L_SIZE_X, &u);
        if (status == X3PIO_OK)
        {
            status = size_of_leaf(s, L_SIZE_Y, &v);
        }
        if (status == X3PIO_OK)
        {
            status = size_of_leaf(s, L_SIZE_Z, &w);
        }
        if (status != X3PIO_OK)
        {
            return status;
        }
        d->dims = (x3pio_dimensions){u, v, w, false};
    }
    else
    {
        size_t count = 0;
        status = size_of_leaf(s, L_LIST, &count);
        if (status != X3PIO_OK)
        {
            return status;
        }
        d->dims = (x3pio_dimensions){count, 1, 1, true};
    }
    /* The data is what counts if the feature type disagrees with it. */
    d->feature = feature;
    if (d->dims.list)
    {
        d->feature = X3PIO_POINT_CLOUD;
    }
    else if (d->feature == X3PIO_POINT_CLOUD
             || (d->feature == X3PIO_PROFILE && d->dims.size_y != 1))
    {
        d->feature = X3PIO_SURFACE;
    }
    text = leaf_text(s, L_REVISION, &size);
    /* A revision that is not known may store its data differently. */
    if (!revision_of_text(text, size, &d->revision))
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT,
                          "The revision is not one that the library reads");
    }
    /* Only Amd 1 allows several vendor identifiers. */
    if (d->vendor_count > 1)
    {
        d->revision = X3PIO_REVISION_AMD1;
    }

    const bool link = s->container_seen[E_DATALINK];
    if (link == s->container_seen[E_DATALIST])
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT,
                          "Record3 needs exactly one of DataLink and DataList");
    }
    if (link)
    {
        if (!have(s, L_POINT_LINK) || leaf_text(s, L_POINT_LINK, &size) == NULL
            || size == 0)
        {
            return x3pio_fail(m, X3PIO_ERR_FORMAT,
                              have(s, L_POINT_LINK) ? "PointDataLink is empty"
                                                    : "DataLink has no "
                                                      "PointDataLink");
        }
        d->binary = true;
        if (!assign(s, &d->point_path, L_POINT_LINK, false)
            || !assign(s, &d->point_md5, L_POINT_MD5, false)
            || !assign(s, &d->valid_path, L_VALID_LINK, false)
            || !assign(s, &d->valid_md5, L_VALID_MD5, false))
        {
            return X3PIO_ERR_MEMORY;
        }
    }
    else
    {
        status = build_datums(s);
        if (status != X3PIO_OK)
        {
            return status;
        }
    }
    text = leaf_text(s, L_CHECKSUM_FILE, &size);
    if (!x3pio_document_assign(d, &d->checksum_file,
                               size > 0 ? text : "md5checksum.hex",
                               size > 0 ? size : strlen("md5checksum.hex")))
    {
        return X3PIO_ERR_MEMORY;
    }
    return X3PIO_OK;
}
