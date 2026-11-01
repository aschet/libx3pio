/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Writing of the points from the buffers of the program.
 */

#include "allocator.h"
#include "matrix.h"
#include "samples.h"
#include "text.h"
#include "vendor.h"
#include "writer.h"
#include <math.h>

x3pio_status x3pio_writer_prepare(x3pio_writer *writer)
{
    const x3pio_allocator *a = &writer->allocator;
    const x3pio_document *d = writer->document;
    const size_t n = writer->chunk;
    size_t record = x3pio_document_record_size(d);
    if (record == 0)
    {
        record = 1;
    }
    writer->bytes = x3pio_allocate_array(a, n, record);
    writer->flag_values = x3pio_allocate_array(a, n, sizeof(double));
    writer->flags = x3pio_allocate(a, n);
    bool ok = writer->bytes != NULL && writer->flag_values != NULL
              && writer->flags != NULL;
    for (size_t k = 0; k < 3; ++k)
    {
        writer->values[k] = x3pio_allocate_array(a, n, sizeof(double));
        ok = ok && writer->values[k] != NULL;
    }
    if (writer->storage == X3PIO_STORAGE_BINARY)
    {
        /* Integers have no NaN: they mark the points that were not measured in
         * a file of bits, one bit for each point. */
        bool integer = false;
        for (size_t k = 0; k < 3; ++k)
        {
            integer = integer
                      || (d->axes[k].type == X3PIO_AXIS_ABSOLUTE
                          && (d->axes[k].storage == X3PIO_TYPE_INT16
                              || d->axes[k].storage == X3PIO_TYPE_INT32));
        }
        if (integer)
        {
            const size_t bytes = (writer->count + 7) / 8;
            writer->bits = x3pio_allocate(a, bytes);
            ok = ok && writer->bits != NULL;
            if (writer->bits != NULL)
            {
                memset(writer->bits, 0xFF, bytes);
            }
        }
    }
    if (writer->storage == X3PIO_STORAGE_TEXT)
    {
        /* The text of all points is kept until the file ends. */
        const size_t absolute = x3pio_document_absolute_count(d);
        writer->text_values =
            x3pio_allocate_array(a, writer->count * absolute, sizeof(double));
        writer->text_valid = x3pio_allocate(a, writer->count);
        ok = ok && writer->text_values != NULL && writer->text_valid != NULL;
        if (writer->text_valid != NULL)
        {
            memset(writer->text_valid, 1, writer->count);
        }
    }
    return ok ? X3PIO_OK
              : x3pio_fail(&writer->message, X3PIO_ERR_MEMORY, "Out of memory");
}

typedef enum mode
{
    MODE_DATA,
    MODE_POINTS,
    MODE_TRANSLATED,
    MODE_GLOBAL
} mode;

static bool metres_type(x3pio_type type)
{
    return type == X3PIO_TYPE_FLOAT || type == X3PIO_TYPE_DOUBLE;
}

/* The buffers of a call, in the order x, y, z, valid. */
typedef struct bound
{
    x3pio_matrix matrix[4];
    bool is_bound[4];
} bound;

static x3pio_status check_fields(x3pio_writer *writer,
                                 const x3pio_const_fields *from, mode m,
                                 size_t rows, bound *b)
{
    const x3pio_document *d = writer->document;
    const x3pio_const_field *fields[4] = {&from->x, &from->y, &from->z,
                                          &from->valid};
    const size_t columns = x3pio_document_columns(d);
    bool any = false;
    for (size_t i = 0; i < 4; ++i)
    {
        b->is_bound[i] = fields[i]->data != NULL;
        if (i < 3)
        {
            const x3pio_axis *axis = &d->axes[i];
            const bool stored = axis->type == X3PIO_AXIS_ABSOLUTE;
            if (m == MODE_GLOBAL)
            {
                /* The transformation acts on a whole point, so the coordinates
                 * of an axis that the file does not store are needed too. */
                if (!b->is_bound[i])
                {
                    return x3pio_fail(
                        &writer->message, X3PIO_ERR_ARGUMENT,
                        "The rotation of the transformation couples "
                        "the axes, so x, y and z have to be "
                        "bound");
                }
            }
            else if (stored && !b->is_bound[i])
            {
                return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                                  "The axis is stored in the file, so its "
                                  "values have to be bound");
            }
            if (m != MODE_GLOBAL && !stored && b->is_bound[i])
            {
                return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                                  "An axis that the file does not store cannot "
                                  "be bound");
            }
            if (b->is_bound[i]
                && !(m == MODE_DATA ? fields[i]->layout.type == axis->storage
                                    : metres_type(fields[i]->layout.type)))
            {
                return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                                  m == MODE_DATA
                                      ? "The type of a layout is not the type "
                                        "in which the file stores the axis"
                                      : "The type of a layout is not float or "
                                        "double");
            }
            if (stored && !(axis->increment > 0.0))
            {
                return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                                  "The increment is not set; "
                                  "x3pio_write_z selects it");
            }
        }
        else if (b->is_bound[i]
                 && (m != MODE_DATA
                     || fields[i]->layout.type != X3PIO_TYPE_UINT8))
        {
            return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                              "The mask of the points is written as bytes by "
                              "x3pio_write_data()");
        }
        if (b->is_bound[i])
        {
            any = true;
            if (!x3pio_matrix_init(&b->matrix[i], fields[i]->data,
                                   &fields[i]->layout, columns, rows, false))
            {
                return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                                  "The type or the strides of a layout are "
                                  "invalid");
            }
        }
    }
    if (!any)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "No field is bound to write from");
    }
    return X3PIO_OK;
}

/* The value as the stored type holds it, so that a file of text reads back what
 * a file of binary data does. */
static double rounded(x3pio_type type, double value)
{
    unsigned char scratch[8];
    double out = 0.0;
    if (x3pio_encode_samples(type, &value, 1, scratch, 8) != X3PIO_OK)
    {
        return value;
    }
    x3pio_decode_samples(type, scratch, 8, &out, 1);
    return out;
}

static void mark_missing(x3pio_writer *writer, size_t index)
{
    writer->missing_seen = true;
    if (writer->storage == X3PIO_STORAGE_BINARY)
    {
        /* A file of floats needs no bits: NaN marks the point. */
        if (writer->bits != NULL)
        {
            writer->bits[index / 8] &= (unsigned char)~(1U << (index % 8));
        }
    }
    else
    {
        writer->text_valid[index] = 0;
    }
}

/* Writes n points taken from the buffers from the point `first` on. */
static x3pio_status write_chunk(x3pio_writer *writer, const bound *b, mode m,
                                size_t first, size_t n)
{
    x3pio_document *d = writer->document;
    for (size_t k = 0; k < 3; ++k)
    {
        if (b->is_bound[k])
        {
            x3pio_matrix_load(&b->matrix[k], first, writer->values[k], n);
        }
    }
    memset(writer->flags, 1, n);
    if (b->is_bound[3])
    {
        x3pio_matrix_load(&b->matrix[3], first, writer->flag_values, n);
        for (size_t i = 0; i < n; ++i)
        {
            writer->flags[i] = writer->flag_values[i] != 0.0 ? 1 : 0;
        }
    }
    for (size_t k = 0; k < 3; ++k)
    {
        if (!b->is_bound[k])
        {
            continue;
        }
        for (size_t i = 0; i < n; ++i)
        {
            if (!isfinite(writer->values[k][i]))
            {
                writer->flags[i] = 0;
            }
        }
    }
    if (m == MODE_TRANSLATED)
    {
        for (size_t k = 0; k < 3; ++k)
        {
            for (size_t i = 0; b->is_bound[k] && i < n; ++i)
            {
                writer->values[k][i] -= d->transformation.offset[k];
            }
        }
    }
    if (m == MODE_GLOBAL)
    {
        for (size_t i = 0; i < n; ++i)
        {
            if (writer->flags[i] == 0)
            {
                continue;
            }
            const double global[3] = {writer->values[0][i],
                                      writer->values[1][i],
                                      writer->values[2][i]};
            double view[3];
            x3pio_transformation_to_view(&d->transformation, global, view);
            for (size_t k = 0; k < 3; ++k)
            {
                writer->values[k][i] = view[k];
            }
        }
    }
    for (size_t k = 0; m != MODE_DATA && k < 3; ++k)
    {
        if (!b->is_bound[k])
        {
            continue;
        }
        for (size_t i = 0; i < n; ++i)
        {
            writer->values[k][i] /= d->axes[k].increment;
        }
    }
    for (size_t i = 0; i < n; ++i)
    {
        if (writer->flags[i] != 0)
        {
            continue;
        }
        if (d->feature == X3PIO_POINT_CLOUD)
        {
            return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                              "A point cloud cannot hold missing points");
        }
        mark_missing(writer, writer->written + i);
        /* A file of floats marks the point by NaN in z. A file of integers
         * keeps the value that the program gave: the mask marks the point
         * there. */
        if (m != MODE_DATA || d->axes[2].storage == X3PIO_TYPE_FLOAT
            || d->axes[2].storage == X3PIO_TYPE_DOUBLE)
        {
            writer->values[2][i] = NAN;
        }
    }
    if (writer->storage == X3PIO_STORAGE_BINARY)
    {
        const size_t record = x3pio_document_record_size(d);
        for (size_t k = 0; k < 3; ++k)
        {
            if (d->axes[k].type != X3PIO_AXIS_ABSOLUTE)
            {
                continue;
            }
            const x3pio_status status = x3pio_encode_samples(
                d->axes[k].storage, writer->values[k], n,
                writer->bytes
                    + x3pio_document_record_offset(d, (x3pio_coordinate)k),
                record);
            if (status == X3PIO_ERR_RANGE)
            {
                return x3pio_fail(&writer->message, status,
                                  "A value does not fit the type of the file");
            }
        }
        if (!writer->data_started)
        {
            const x3pio_status status = x3pio_zip_writer_begin(
                writer->zip, X3PIO_DATA_FILE,
                (uint64_t)x3pio_document_point_count(d) * record,
                &writer->compression, &writer->message);
            if (status != X3PIO_OK)
            {
                return status;
            }
            writer->data_started = true;
        }
        const x3pio_status status = x3pio_zip_writer_write(
            writer->zip, writer->bytes, n * record, &writer->message);
        if (status == X3PIO_OK)
        {
            x3pio_md5_update(&writer->hash, writer->bytes, n * record);
        }
        return status;
    }
    const size_t absolute = x3pio_document_absolute_count(d);
    size_t column = 0;
    for (size_t k = 0; k < 3; ++k)
    {
        if (d->axes[k].type != X3PIO_AXIS_ABSOLUTE)
        {
            continue;
        }
        for (size_t i = 0; i < n; ++i)
        {
            writer->text_values[(writer->written + i) * absolute + column] =
                writer->flags[i] != 0
                    ? rounded(d->axes[k].storage, writer->values[k][i])
                    : NAN;
        }
        ++column;
    }
    return X3PIO_OK;
}

static x3pio_status write_rows(x3pio_writer *writer,
                               const x3pio_const_fields *from, size_t points,
                               mode m)
{
    if (writer == NULL || from == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (!writer->created || writer->ended || writer->failed)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_STATE,
                          "No file is created, the file is ended, or an "
                          "earlier error ended the writing");
    }
    const size_t columns = x3pio_document_columns(writer->document);
    if (points % columns != 0)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "A matrix is written in runs of SizeX points along "
                          "u");
    }
    const size_t rows = points / columns;
    if (points > writer->count - writer->written)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "More points than the file has room for");
    }
    bound b;
    x3pio_status status =
        check_fields(writer, from, m, rows != 0 ? rows : 1, &b);
    if (status != X3PIO_OK || rows == 0)
    {
        return status;
    }
    const size_t count = points;
    for (size_t done = 0; done < count; done += writer->chunk)
    {
        const size_t n =
            count - done < writer->chunk ? count - done : writer->chunk;
        status = write_chunk(writer, &b, m, done, n);
        if (status != X3PIO_OK)
        {
            writer->failed = true;
            return status;
        }
        writer->written += n;
    }
    return X3PIO_OK;
}

x3pio_status x3pio_write_data(x3pio_writer *writer,
                              const x3pio_const_fields *from, size_t points)
{
    return write_rows(writer, from, points, MODE_DATA);
}

x3pio_status x3pio_write_points(x3pio_writer *writer,
                                const x3pio_const_fields *from, size_t points)
{
    return write_rows(writer, from, points, MODE_POINTS);
}

x3pio_status x3pio_write_points_in(x3pio_writer *writer,
                                   x3pio_coordinate_system system,
                                   const x3pio_const_fields *from,
                                   size_t points)
{
    switch (system)
    {
    case X3PIO_COORDINATES_VIEW:
        return write_rows(writer, from, points, MODE_POINTS);
    case X3PIO_COORDINATES_VIEW_OFFSET:
        return write_rows(writer, from, points, MODE_TRANSLATED);
    case X3PIO_COORDINATES_GLOBAL:
        return write_rows(writer, from, points, MODE_GLOBAL);
    }
    return writer != NULL ? x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                                       "The coordinate system is not defined")
                          : X3PIO_ERR_ARGUMENT;
}

x3pio_status x3pio_write_z(x3pio_writer *writer, const x3pio_layout *layout,
                           const void *src)
{
    if (writer == NULL || layout == NULL || src == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (!writer->created || writer->ended || writer->failed
        || writer->written != 0)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_STATE,
                          "No file is created, an earlier error ended the "
                          "writing, or points were written already");
    }
    x3pio_document *d = writer->document;
    if (d->axes[0].type == X3PIO_AXIS_ABSOLUTE
        || d->axes[1].type == X3PIO_AXIS_ABSOLUTE)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "The file stores x and y, so x3pio_write_points "
                          "writes it");
    }
    if (!metres_type(layout->type))
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "The type of a layout is not float or double");
    }
    const size_t columns = x3pio_document_columns(d);
    const size_t rows = x3pio_document_rows(d);
    if (d->axes[2].increment == X3PIO_AUTO_INCREMENT)
    {
        double increment = 1.0;
        const x3pio_status status = x3pio_suggest_increment(
            d->axes[2].storage, layout, src, rows, columns, &increment);
        if (status != X3PIO_OK)
        {
            return x3pio_fail(&writer->message, status,
                              "The type or the strides of a layout are "
                              "invalid");
        }
        d->axes[2].increment = increment;
    }
    x3pio_const_fields from;
    x3pio_const_fields_init(&from);
    from.z = (x3pio_const_field){src, *layout};
    return write_rows(writer, &from, x3pio_document_point_count(d),
                      MODE_POINTS);
}
