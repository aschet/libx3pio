/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Life of a reader: creation, options, opening, release, and the members
 * of the container.
 */

#include <x3pio/file.h>

#include "allocator.h"
#include "reader.h"
#include "text.h"
#include "vendor.h"
#include "xml.h"

x3pio_status x3pio_reader_create(x3pio_reader **reader)
{
    return x3pio_reader_create_with(NULL, reader);
}

x3pio_status x3pio_reader_create_with(const x3pio_allocator *allocator,
                                      x3pio_reader **reader)
{
    if (reader == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    *reader = NULL;
    x3pio_allocator used;
    if (!x3pio_allocator_take(allocator, &used))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    x3pio_reader *handle = x3pio_allocate(&used, sizeof *handle);
    if (handle == NULL)
    {
        return X3PIO_ERR_MEMORY;
    }
    *handle = (x3pio_reader){.allocator = used,
                             .message = "",
                             .verify = true,
                             .chunk = X3PIO_DEFAULT_CHUNK};
    *reader = handle;
    return X3PIO_OK;
}

static x3pio_status require_new(x3pio_reader *reader)
{
    if (reader == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (reader->opened)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_STATE,
                          "Options can be set only before a file is opened");
    }
    return X3PIO_OK;
}

x3pio_status x3pio_reader_set_verify(x3pio_reader *reader, bool verify)
{
    const x3pio_status status = require_new(reader);
    if (status == X3PIO_OK)
    {
        reader->verify = verify;
    }
    return status;
}

x3pio_status x3pio_reader_set_chunk(x3pio_reader *reader, size_t count)
{
    x3pio_status status = require_new(reader);
    if (status == X3PIO_OK && !x3pio_chunk_valid(count))
    {
        status = x3pio_fail(&reader->message, X3PIO_ERR_ARGUMENT,
                            "The number of points of a chunk is 0 or too "
                            "large");
    }
    if (status == X3PIO_OK)
    {
        reader->chunk = count;
    }
    return status;
}

static void release_vendors(x3pio_reader *reader)
{
    for (size_t i = 0; i < reader->vendor_count; ++i)
    {
        x3pio_release(&reader->allocator, reader->vendors[i].id);
        x3pio_release(&reader->allocator, reader->vendors[i].member);
    }
    x3pio_release(&reader->allocator, reader->vendors);
    reader->vendors = NULL;
    reader->vendor_count = 0;
}

void x3pio_reader_reset(x3pio_reader *reader)
{
    const x3pio_allocator *a = &reader->allocator;
    x3pio_zip_entry_close(reader->entry);
    reader->entry = NULL;
    release_vendors(reader);
    x3pio_member_index_free(&reader->index);
    x3pio_document_free(reader->document);
    reader->document = NULL;
    x3pio_release(a, reader->path);
    x3pio_release(a, reader->point_name);
    x3pio_release(a, reader->valid_bits);
    x3pio_release(a, reader->bytes);
    x3pio_release(a, reader->flag_values);
    x3pio_release(a, reader->flags);
    for (size_t k = 0; k < 3; ++k)
    {
        x3pio_release(a, reader->raw[k]);
        x3pio_release(a, reader->values[k]);
        reader->raw[k] = NULL;
        reader->values[k] = NULL;
    }
    reader->path = NULL;
    reader->point_name = NULL;
    reader->valid_bits = NULL;
    reader->valid_size = 0;
    reader->bytes = NULL;
    reader->flag_values = NULL;
    reader->flags = NULL;
    reader->source = (x3pio_zip_source){0};
    reader->rows_read = 0;
    reader->failed = false;
    reader->opened = false;
}

void x3pio_reader_free(x3pio_reader *reader)
{
    if (reader == NULL)
    {
        return;
    }
    x3pio_reader_reset(reader);
    const x3pio_allocator allocator = reader->allocator;
    x3pio_release(&allocator, reader);
}

const x3pio_document *x3pio_reader_document(const x3pio_reader *reader)
{
    return reader != NULL && reader->opened ? reader->document : NULL;
}

const char *x3pio_reader_error_message(const x3pio_reader *reader)
{
    return reader != NULL ? reader->message : "";
}

/* Opens the zip file that the source names. */
static x3pio_status open_source(x3pio_reader *reader)
{
    char **names = NULL;
    size_t name_count = 0;
    x3pio_status status =
        x3pio_zip_names(&reader->source, &reader->allocator, &names,
                        &name_count, &reader->message);
    if (status != X3PIO_OK)
    {
        return status;
    }
    status = x3pio_member_index_build(&reader->index, &reader->allocator, names,
                                      name_count, &reader->message);
    x3pio_zip_names_free(&reader->allocator, names, name_count);
    if (status == X3PIO_ERR_MEMORY)
    {
        return x3pio_fail(&reader->message, status, "Out of memory");
    }
    if (status != X3PIO_OK)
    {
        return status;
    }

    status = x3pio_reader_load_document(reader);
    if (status != X3PIO_OK)
    {
        return status;
    }
    if (reader->document->binary)
    {
        status = x3pio_reader_open_binary_files(reader);
        if (status != X3PIO_OK)
        {
            return status;
        }
    }
    return x3pio_reader_find_vendor_files(reader);
}

/* Opens what the source of the reader names, and cleans up if that fails. */
static x3pio_status open_and_keep(x3pio_reader *reader)
{
    reader->message = "";
    x3pio_status status = open_source(reader);
    if (status == X3PIO_OK)
    {
        status = x3pio_reader_prepare(reader);
    }
    if (status != X3PIO_OK)
    {
        const char *message = reader->message;
        x3pio_reader_reset(reader);
        reader->message = message;
        return status;
    }
    x3pio_md5_init(&reader->hash);
    reader->opened = true;
    return X3PIO_OK;
}

static x3pio_status require_closed(x3pio_reader *reader)
{
    if (reader->opened)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_STATE,
                          "The reader has opened a file already");
    }
    return X3PIO_OK;
}

x3pio_status x3pio_open_memory(x3pio_reader *reader, const void *data,
                               size_t size)
{
    if (reader == NULL || (data == NULL && size != 0))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    const x3pio_status ready = require_closed(reader);
    if (ready != X3PIO_OK)
    {
        return ready;
    }
    reader->source = (x3pio_zip_source){NULL, data, size};
    return open_and_keep(reader);
}

x3pio_status x3pio_open_file(x3pio_reader *reader, const char *path)
{
    if (reader == NULL || path == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    const x3pio_status ready = require_closed(reader);
    if (ready != X3PIO_OK)
    {
        return ready;
    }
    reader->path = x3pio_strdup(&reader->allocator, path);
    if (reader->path == NULL)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    reader->source = (x3pio_zip_source){reader->path, NULL, 0};
    return open_and_keep(reader);
}
