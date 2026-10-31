/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Reading of the rows of points into the buffers of the program.
 */

#include "allocator.h"
#include "codec.h"
#include "reader.h"
#include "text.h"
#include <math.h>

x3pio_status x3pio_reader_prepare(x3pio_reader *reader)
{
    const x3pio_allocator *a = &reader->allocator;
    const size_t n = reader->chunk;
    size_t record = x3pio_document_record_size(reader->document);
    if (record == 0)
    {
        record = 1;
    }
    reader->bytes = x3pio_allocate_array(a, n, record);
    reader->flag_values = x3pio_allocate_array(a, n, sizeof(double));
    reader->flags = x3pio_allocate(a, n);
    bool ok = reader->bytes != NULL && reader->flag_values != NULL
              && reader->flags != NULL;
    for (size_t k = 0; k < 3; ++k)
    {
        reader->raw[k] = x3pio_allocate_array(a, n, sizeof(double));
        reader->values[k] = x3pio_allocate_array(a, n, sizeof(double));
        ok = ok && reader->raw[k] != NULL && reader->values[k] != NULL;
    }
    if (!ok)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    /* The member with the points is opened now, so that reading allocates
     * nothing. */
    if (reader->document->binary)
    {
        const x3pio_status status = x3pio_zip_entry_open(
            &reader->source, &reader->allocator, reader->point_name,
            &reader->entry, &reader->message);
        if (status != X3PIO_OK)
        {
            return status;
        }
        x3pio_md5_init(&reader->hash);
    }
    return X3PIO_OK;
}

static x3pio_status read_exactly(x3pio_reader *reader, unsigned char *dst,
                                 size_t size)
{
    size_t done = 0;
    while (done < size)
    {
        size_t got = 0;
        const x3pio_status status = x3pio_zip_entry_read(
            reader->entry, dst + done, size - done, &got, &reader->message);
        if (status != X3PIO_OK)
        {
            return status;
        }
        if (got == 0)
        {
            return x3pio_fail(&reader->message, X3PIO_ERR_FORMAT,
                              "The point data ends before all points are "
                              "stored");
        }
        done += got;
    }
    return X3PIO_OK;
}

/* Decodes n points from the point `first` into raw and flags. */
static x3pio_status decode(x3pio_reader *reader, size_t first, size_t n)
{
    const x3pio_document *d = reader->document;
    const size_t absolute = x3pio_document_absolute_count(d);
    if (d->binary)
    {
        const size_t record = x3pio_document_record_size(d);
        const x3pio_status status =
            read_exactly(reader, reader->bytes, n * record);
        if (status != X3PIO_OK)
        {
            return status;
        }
        x3pio_md5_update(&reader->hash, reader->bytes, n * record);
        for (size_t k = 0; k < 3; ++k)
        {
            if (d->axes[k].type == X3PIO_AXIS_ABSOLUTE)
            {
                x3pio_decode_samples(
                    d->axes[k].storage,
                    reader->bytes
                        + x3pio_document_record_offset(d, (x3pio_coordinate)k),
                    record, reader->raw[k], n);
            }
        }
    }
    else
    {
        size_t column = 0;
        for (size_t k = 0; k < 3; ++k)
        {
            if (d->axes[k].type != X3PIO_AXIS_ABSOLUTE)
            {
                continue;
            }
            for (size_t i = 0; i < n; ++i)
            {
                reader->raw[k][i] =
                    d->text_values[(first + i) * absolute + column];
            }
            ++column;
        }
    }
    memset(reader->flags, 1, n);
    if (reader->valid_bits != NULL)
    {
        for (size_t i = 0; i < n; ++i)
        {
            const size_t j = first + i;
            reader->flags[i] =
                (unsigned char)((reader->valid_bits[j / 8] >> (j % 8U)) & 1U);
        }
    }
    if (d->text_valid != NULL)
    {
        for (size_t i = 0; i < n; ++i)
        {
            reader->flags[i] &= d->text_valid[first + i];
        }
    }
    /* A point with a coordinate that is not a number is missing. */
    for (size_t k = 0; k < 3; ++k)
    {
        if (d->axes[k].type != X3PIO_AXIS_ABSOLUTE)
        {
            continue;
        }
        for (size_t i = 0; i < n; ++i)
        {
            if (!isfinite(reader->raw[k][i]))
            {
                reader->flags[i] = 0;
            }
        }
    }
    return X3PIO_OK;
}

static x3pio_status finish_points(x3pio_reader *reader)
{
    const x3pio_document *d = reader->document;
    if (!d->binary || !reader->verify || d->point_md5 == NULL
        || d->point_md5[0] == '\0')
    {
        return X3PIO_OK;
    }
    char computed[X3PIO_MD5_HEX_SIZE];
    x3pio_md5_hex(&reader->hash, computed);
    bool same = strlen(d->point_md5) == 32;
    for (size_t i = 0; same && i < 32; ++i)
    {
        same = x3pio_to_lower(d->point_md5[i]) == computed[i];
    }
    if (!same)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_CHECKSUM,
                          "The checksum of the point data does not match");
    }
    return X3PIO_OK;
}

/* The buffers of a call, in the order x, y, z, valid. */
typedef struct bound
{
    x3pio_matrix matrix[4];
    bool is_bound[4];
} bound;

typedef enum mode
{
    MODE_DATA,
    MODE_POINTS
} mode;

static bool metres_type(x3pio_type type)
{
    return type == X3PIO_TYPE_FLOAT || type == X3PIO_TYPE_DOUBLE;
}

static x3pio_status check_fields(x3pio_reader *reader, const x3pio_fields *into,
                                 mode m, size_t rows, bound *b)
{
    const x3pio_document *d = reader->document;
    const x3pio_field *fields[4] = {&into->x, &into->y, &into->z, &into->valid};
    const size_t columns = x3pio_document_columns(d);
    bool any = false;
    for (size_t i = 0; i < 4; ++i)
    {
        b->is_bound[i] = fields[i]->data != NULL;
        if (!b->is_bound[i])
        {
            continue;
        }
        any = true;
        if (i < 3)
        {
            const x3pio_axis *axis = &d->axes[i];
            if (m == MODE_DATA && axis->type != X3PIO_AXIS_ABSOLUTE)
            {
                return x3pio_fail(&reader->message, X3PIO_ERR_ARGUMENT,
                                  "An axis that the file does not store cannot "
                                  "be bound");
            }
            const bool type_ok = m == MODE_DATA
                                     ? fields[i]->layout.type == axis->storage
                                     : metres_type(fields[i]->layout.type);
            if (!type_ok)
            {
                return x3pio_fail(&reader->message, X3PIO_ERR_ARGUMENT,
                                  m == MODE_DATA
                                      ? "The type of a layout is not the type "
                                        "in which the file stores the axis"
                                      : "The type of a layout is not float or "
                                        "double");
            }
        }
        else if (m != MODE_DATA || fields[i]->layout.type != X3PIO_TYPE_UINT8)
        {
            return x3pio_fail(&reader->message, X3PIO_ERR_ARGUMENT,
                              "The mask of the points is read as bytes by "
                              "x3pio_read_data()");
        }
        if (!x3pio_matrix_make(&b->matrix[i], fields[i]->data,
                               &fields[i]->layout, columns, rows, true))
        {
            return x3pio_fail(
                &reader->message, X3PIO_ERR_ARGUMENT,
                "The type or the strides of a layout are invalid");
        }
    }
    if (!any)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_ARGUMENT,
                          "No field is bound to read into");
    }
    return X3PIO_OK;
}

/* Turns the decoded values of a chunk into what the call asks for. */
static void convert(x3pio_reader *reader, const bound *b, mode m, size_t first,
                    size_t n)
{
    const x3pio_document *d = reader->document;
    for (size_t k = 0; k < 3; ++k)
    {
        if (!b->is_bound[k])
        {
            continue;
        }
        double *out = reader->values[k];
        const x3pio_axis *axis = &d->axes[k];
        if (m == MODE_DATA)
        {
            /* The numbers of the file, as they are. */
            memcpy(out, reader->raw[k], n * sizeof *out);
        }
        else if (axis->type == X3PIO_AXIS_ABSOLUTE)
        {
            const double *raw = reader->raw[k];
            for (size_t i = 0; i < n; ++i)
            {
                out[i] = reader->flags[i] != 0 ? raw[i] * axis->increment : NAN;
            }
        }
        else
        {
            for (size_t i = 0; i < n; ++i)
            {
                const size_t at = first + i;
                const size_t index =
                    k == 0 ? at % d->dims.size_x
                           : (at / d->dims.size_x) % d->dims.size_y;
                out[i] = (double)index * axis->increment;
            }
        }
    }
}

static x3pio_status read_rows(x3pio_reader *reader, const x3pio_fields *into,
                              size_t rows, mode m)
{
    if (reader == NULL || into == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (!reader->opened || reader->failed)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_STATE,
                          reader->opened ? "An earlier error ended the reading"
                                         : "No file is open");
    }
    const size_t total_rows = x3pio_document_rows(reader->document);
    if (rows > total_rows - reader->rows_read)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_ARGUMENT,
                          "More rows than the file has left");
    }
    bound b;
    x3pio_status status =
        check_fields(reader, into, m, rows != 0 ? rows : 1, &b);
    if (status != X3PIO_OK || rows == 0)
    {
        return status;
    }
    const size_t columns = x3pio_document_columns(reader->document);
    const size_t first_point = reader->rows_read * columns;
    const size_t count = rows * columns;
    for (size_t done = 0; done < count; done += reader->chunk)
    {
        const size_t n =
            count - done < reader->chunk ? count - done : reader->chunk;
        status = decode(reader, first_point + done, n);
        if (status == X3PIO_OK)
        {
            convert(reader, &b, m, first_point + done, n);
            for (size_t k = 0; k < 3 && status == X3PIO_OK; ++k)
            {
                if (b.is_bound[k])
                {
                    status = x3pio_matrix_store(&b.matrix[k], done,
                                                reader->values[k], n);
                }
            }
            if (status == X3PIO_ERR_RANGE)
            {
                status = x3pio_fail(&reader->message, status,
                                    "A value does not fit the type of its "
                                    "buffer");
            }
        }
        if (status == X3PIO_OK && b.is_bound[3])
        {
            for (size_t i = 0; i < n; ++i)
            {
                reader->flag_values[i] = reader->flags[i];
            }
            status =
                x3pio_matrix_store(&b.matrix[3], done, reader->flag_values, n);
        }
        if (status != X3PIO_OK)
        {
            reader->failed = true;
            return status;
        }
    }
    reader->rows_read += rows;
    if (reader->rows_read == total_rows)
    {
        /* The checksum of the zip file, and the one of main.xml. */
        if (reader->entry != NULL)
        {
            status = x3pio_zip_entry_finish(reader->entry, &reader->message);
            reader->entry = NULL;
        }
        if (status == X3PIO_OK)
        {
            status = finish_points(reader);
        }
        if (status != X3PIO_OK)
        {
            reader->failed = true;
        }
    }
    return status;
}

x3pio_status x3pio_read_data(x3pio_reader *reader, const x3pio_fields *into,
                             size_t rows)
{
    return read_rows(reader, into, rows, MODE_DATA);
}

x3pio_status x3pio_read_points(x3pio_reader *reader, const x3pio_fields *into,
                               size_t rows)
{
    return read_rows(reader, into, rows, MODE_POINTS);
}

x3pio_status x3pio_read_heights(x3pio_reader *reader,
                                const x3pio_layout *layout, void *dst)
{
    if (reader == NULL || layout == NULL || dst == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (reader->opened && !reader->failed && reader->rows_read != 0)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_STATE,
                          "Rows were read already");
    }
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.z = (x3pio_field){dst, *layout};
    return read_rows(reader, &into, x3pio_document_rows(reader->document),
                     MODE_POINTS);
}
