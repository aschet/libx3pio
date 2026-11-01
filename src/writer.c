/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Life of a writer: creation, options, the files of vendors, the end.
 */

#include <x3pio/file.h>

#include "allocator.h"
#include "text.h"
#include "vendor.h"
#include "writer.h"
#include "xml.h"

x3pio_status x3pio_writer_create(x3pio_writer **writer)
{
    return x3pio_writer_create_with(NULL, writer);
}

x3pio_status x3pio_writer_create_with(const x3pio_allocator *allocator,
                                      x3pio_writer **writer)
{
    if (writer == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    *writer = NULL;
    x3pio_allocator used;
    if (!x3pio_allocator_take(allocator, &used))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    x3pio_writer *handle = x3pio_allocate(&used, sizeof *handle);
    if (handle == NULL)
    {
        return X3PIO_ERR_MEMORY;
    }
    *handle = (x3pio_writer){.allocator = used,
                             .message = "",
                             .storage = X3PIO_STORAGE_BINARY,
                             .compression = x3pio_compression_default(),
                             .chunk = X3PIO_DEFAULT_CHUNK};
    x3pio_buffer_init(&handle->memory, &handle->allocator);
    handle->sink = &handle->memory;
    *writer = handle;
    return X3PIO_OK;
}

static x3pio_status require_new(x3pio_writer *writer)
{
    if (writer == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (writer->created)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_STATE,
                          "Options can be set only before a file is created");
    }
    return X3PIO_OK;
}

x3pio_status x3pio_writer_set_storage(x3pio_writer *writer,
                                      x3pio_storage storage)
{
    x3pio_status status = require_new(writer);
    if (status == X3PIO_OK && storage != X3PIO_STORAGE_BINARY
        && storage != X3PIO_STORAGE_TEXT)
    {
        status = x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                            "The storage is not one of the enumerators");
    }
    if (status == X3PIO_OK)
    {
        writer->storage = storage;
    }
    return status;
}

bool x3pio_compression_valid(const x3pio_compression *compression)
{
    return (compression->method == X3PIO_COMPRESS_STORE
            || (compression->method == X3PIO_COMPRESS_DEFLATE
                && compression->level >= -1 && compression->level <= 9));
}

x3pio_status x3pio_writer_set_compression(x3pio_writer *writer,
                                          const x3pio_compression *compression)
{
    if (writer == NULL || compression == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    x3pio_status status = require_new(writer);
    if (status == X3PIO_OK && !x3pio_compression_valid(compression))
    {
        status = x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                            "The compression level is not one of -1 (the "
                            "default) and 0 to 9, or the method is unknown");
    }
    if (status == X3PIO_OK)
    {
        writer->compression = *compression;
    }
    return status;
}

x3pio_status x3pio_writer_set_chunk(x3pio_writer *writer, size_t count)
{
    x3pio_status status = require_new(writer);
    if (status == X3PIO_OK && !x3pio_chunk_valid(count))
    {
        status = x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                            "The number of points of a chunk is 0 or too "
                            "large");
    }
    if (status == X3PIO_OK)
    {
        writer->chunk = count;
    }
    return status;
}

const char *x3pio_writer_error_message(const x3pio_writer *writer)
{
    return writer != NULL ? writer->message : "";
}

static void release_all(x3pio_writer *writer)
{
    const x3pio_allocator *a = &writer->allocator;
    x3pio_zip_writer_free(writer->zip);
    writer->zip = NULL;
    x3pio_document_free(writer->document);
    writer->document = NULL;
    x3pio_buffer_free(&writer->memory);
    x3pio_release(a, writer->bits);
    x3pio_release(a, writer->text_values);
    x3pio_release(a, writer->text_valid);
    for (size_t i = 0; i < writer->vendor_id_count; ++i)
    {
        x3pio_release(a, writer->vendor_ids[i]);
    }
    x3pio_release_strings(a, writer->vendor_ids);
    for (size_t i = 0; i < writer->vendor_file_count; ++i)
    {
        x3pio_release(a, writer->vendor_files[i].name);
        x3pio_release(a, writer->vendor_files[i].content);
    }
    x3pio_release(a, writer->vendor_files);
    x3pio_release(a, writer->bytes);
    x3pio_release(a, writer->flag_values);
    x3pio_release(a, writer->flags);
    for (size_t k = 0; k < 3; ++k)
    {
        x3pio_release(a, writer->values[k]);
    }
    writer->bits = NULL;
    writer->text_values = NULL;
    writer->text_valid = NULL;
    writer->vendor_ids = NULL;
    writer->vendor_id_count = 0;
    writer->vendor_files = NULL;
    writer->vendor_file_count = 0;
    writer->bytes = NULL;
    writer->flag_values = NULL;
    writer->flags = NULL;
    for (size_t k = 0; k < 3; ++k)
    {
        writer->values[k] = NULL;
    }
}

void x3pio_writer_free(x3pio_writer *writer)
{
    if (writer == NULL)
    {
        return;
    }
    release_all(writer);
    const x3pio_allocator allocator = writer->allocator;
    x3pio_release(&allocator, writer);
}

/* ---- creation --------------------------------------------------------------
 */

/* Checks and copies the document. */
static x3pio_status prepare(x3pio_writer *writer,
                            const x3pio_document *document)
{
    const char *reason = "";
    if (x3pio_document_validate(document, &reason) != X3PIO_OK)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT, reason);
    }
    writer->document = x3pio_document_allocate(&writer->allocator);
    if (writer->document == NULL)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    if (x3pio_document_copy(writer->document, document) != X3PIO_OK)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    /* The points, the links and the vendors of the document are not used. */
    x3pio_document_clear_data(writer->document);
    writer->count = x3pio_document_point_count(writer->document);
    x3pio_md5_init(&writer->hash);
    return x3pio_writer_prepare(writer);
}

static x3pio_status begin(x3pio_writer *writer, const x3pio_document *document,
                          x3pio_target target)
{
    if (writer == NULL || document == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (writer->created)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_STATE,
                          "The writer has created a file already");
    }
    writer->message = "";
    x3pio_status status = prepare(writer, document);
    if (status == X3PIO_OK && target != X3PIO_TARGET_FILE)
    {
        status = x3pio_zip_writer_open_memory(&writer->allocator, writer->sink,
                                              &writer->zip, &writer->message);
    }
    if (status != X3PIO_OK)
    {
        const char *message = writer->message;
        release_all(writer);
        writer->message = message;
        return status;
    }
    writer->target = target;
    writer->created = true;
    return X3PIO_OK;
}

x3pio_status x3pio_create_memory_into(x3pio_writer *writer,
                                      const x3pio_document *document,
                                      x3pio_buffer *sink)
{
    if (writer == NULL || sink == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (writer->created)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_STATE,
                          "The writer has created a file already");
    }
    sink->size = 0;
    writer->sink = sink;
    const x3pio_status status = begin(writer, document, X3PIO_TARGET_MEMORY);
    if (status != X3PIO_OK)
    {
        writer->sink = &writer->memory;
    }
    return status;
}

x3pio_status x3pio_create_memory(x3pio_writer *writer,
                                 const x3pio_document *document)
{
    return writer != NULL
               ? x3pio_create_memory_into(writer, document, &writer->memory)
               : X3PIO_ERR_ARGUMENT;
}

x3pio_status x3pio_writer_memory(const x3pio_writer *writer, const void **data,
                                 size_t *size)
{
    if (writer == NULL || data == NULL || size == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (writer->target != X3PIO_TARGET_MEMORY || !writer->ended)
    {
        return X3PIO_ERR_STATE;
    }
    *data = writer->sink->data;
    *size = writer->sink->size;
    return X3PIO_OK;
}

x3pio_status x3pio_create_file(x3pio_writer *writer, const char *path,
                               const x3pio_document *document)
{
    if (writer == NULL || path == NULL || document == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (writer->created)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_STATE,
                          "The writer has created a file already");
    }
    /* A document that cannot be written creates no file: check it first. */
    const char *reason = "";
    if (x3pio_document_validate(document, &reason) != X3PIO_OK)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT, reason);
    }
    x3pio_status status = begin(writer, document, X3PIO_TARGET_FILE);
    if (status != X3PIO_OK)
    {
        return status;
    }
    status = x3pio_zip_writer_open_file(&writer->allocator, path, &writer->zip,
                                        &writer->message);
    if (status != X3PIO_OK)
    {
        const char *message = writer->message;
        release_all(writer);
        writer->created = false;
        writer->target = X3PIO_TARGET_NONE;
        writer->message = message;
    }
    return status;
}
