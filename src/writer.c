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

static bool compression_valid(const x3pio_compression *compression)
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
    if (status == X3PIO_OK && !compression_valid(compression))
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
    writer->document = x3pio_document_make(&writer->allocator);
    if (writer->document == NULL)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    if (x3pio_document_copy(writer->document, document) != X3PIO_OK)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    /* The points, the links and the vendors of the description are not used. */
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

/* ---- the files of vendors
 * ----------------------------------------------------- */

static bool has_id(const x3pio_writer *writer, const char *id)
{
    for (size_t i = 0; i < writer->vendor_id_count; ++i)
    {
        if (strcmp(writer->vendor_ids[i], id) == 0)
        {
            return true;
        }
    }
    return false;
}

static x3pio_status add_vendor_checked(x3pio_writer *writer, const char *id,
                                       const char *name, const void *content,
                                       size_t size,
                                       const x3pio_compression *compression)
{
    const x3pio_allocator *a = &writer->allocator;
    char *key = NULL;
    x3pio_status status = x3pio_member_path(a, name, strlen(name), &key);
    if (status == X3PIO_ERR_MEMORY)
    {
        return x3pio_fail(&writer->message, status, "Out of memory");
    }
    if (status != X3PIO_OK)
    {
        return x3pio_fail(
            &writer->message, X3PIO_ERR_ARGUMENT,
            "The name does not point to a member of the container");
    }
    if (x3pio_is_reserved_name(key))
    {
        x3pio_release(a, key);
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "The name is one that the format uses");
    }
    for (size_t i = 0; i < writer->vendor_file_count; ++i)
    {
        char *other = NULL;
        const x3pio_status other_status =
            x3pio_member_path(a, writer->vendor_files[i].name,
                              strlen(writer->vendor_files[i].name), &other);
        if (other_status == X3PIO_ERR_MEMORY)
        {
            x3pio_release(a, key);
            return x3pio_fail(&writer->message, other_status, "Out of memory");
        }
        if (other_status == X3PIO_OK)
        {
            const bool same = strcmp(other, key) == 0;
            x3pio_release(a, other);
            if (same)
            {
                x3pio_release(a, key);
                return x3pio_fail(
                    &writer->message, X3PIO_ERR_ARGUMENT,
                    "The container has a file of that name already");
            }
        }
    }
    x3pio_release(a, key);

    /* Nothing was refused: commit. */
    const bool known = has_id(writer, id);
    char *id_copy = known ? NULL : x3pio_strdup(a, id);
    char *name_copy = x3pio_strdup(a, name);
    unsigned char *content_copy = x3pio_allocate(a, size);
    char **ids = x3pio_allocate_strings(a, writer->vendor_id_count + 1);
    x3pio_vendor_entry *files =
        x3pio_allocate_array(a, writer->vendor_file_count + 1, sizeof *files);
    if ((!known && id_copy == NULL) || name_copy == NULL || content_copy == NULL
        || ids == NULL || files == NULL)
    {
        x3pio_release(a, id_copy);
        x3pio_release(a, name_copy);
        x3pio_release(a, content_copy);
        x3pio_release_strings(a, ids);
        x3pio_release(a, files);
        return x3pio_fail(&writer->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    if (size > 0)
    {
        memcpy(content_copy, content, size);
    }
    x3pio_copy_strings(ids, writer->vendor_ids, writer->vendor_id_count);
    if (writer->vendor_file_count > 0)
    {
        memcpy(files, writer->vendor_files,
               writer->vendor_file_count * sizeof *files);
    }
    x3pio_release_strings(a, writer->vendor_ids);
    x3pio_release(a, writer->vendor_files);
    writer->vendor_ids = ids;
    writer->vendor_files = files;
    if (!known)
    {
        writer->vendor_ids[writer->vendor_id_count++] = id_copy;
    }
    writer->vendor_files[writer->vendor_file_count++] = (x3pio_vendor_entry){
        name_copy, content_copy, size,
        compression != NULL ? *compression : writer->compression};
    return X3PIO_OK;
}

x3pio_status x3pio_add_vendor_file(x3pio_writer *writer, const char *id,
                                   const char *member, const void *content,
                                   size_t size,
                                   const x3pio_compression *compression)
{
    if (writer == NULL || id == NULL || (content == NULL && size != 0))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (!writer->created || writer->ended || writer->failed)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_STATE,
                          "No file is created, or it is ended");
    }
    if (compression != NULL && !compression_valid(compression))
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "The compression level is not one of -1 (the "
                          "default) and 0 to 9, or the method is unknown");
    }
    const bool amd1 = writer->document->revision == X3PIO_REVISION_AMD1;
    const bool known = has_id(writer, id);
    if (amd1)
    {
        if (known)
        {
            return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                              "The vendor id is registered already");
        }
        if (member == NULL || member[0] == '\0')
        {
            char *name = NULL;
            const x3pio_status status =
                x3pio_vendor_member(&writer->allocator, id, strlen(id), &name);
            if (status == X3PIO_ERR_MEMORY)
            {
                return x3pio_fail(&writer->message, status, "Out of memory");
            }
            if (status != X3PIO_OK)
            {
                return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                                  "A vendor id must be a URI with a host name "
                                  "and a path to a file");
            }
            const x3pio_status added = add_vendor_checked(
                writer, id, name, content, size, compression);
            x3pio_release(&writer->allocator, name);
            return added;
        }
    }
    else
    {
        if (member == NULL || member[0] == '\0')
        {
            return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                              "ISO 25178-72:2017 needs the name of the file in "
                              "the container");
        }
        if (id[0] == '\0' || (writer->vendor_id_count > 0 && !known))
        {
            return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                              "ISO 25178-72:2017 allows one vendor id for all "
                              "files");
        }
    }
    return add_vendor_checked(writer, id, member, content, size, compression);
}

/* ---- the end
 * ------------------------------------------------------------------- */

/* Writes a member that is in memory. */
static x3pio_status put_member(x3pio_writer *writer, const char *name,
                               const void *data, size_t size,
                               const x3pio_compression *compression)
{
    x3pio_status status = x3pio_zip_writer_begin(writer->zip, name, compression,
                                                 &writer->message);
    if (status == X3PIO_OK)
    {
        status =
            x3pio_zip_writer_write(writer->zip, data, size, &writer->message);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_zip_writer_end(writer->zip, &writer->message);
    }
    return status;
}

/* The bytes of main.xml go to the zip file and to the checksum. */
typedef struct main_sink
{
    x3pio_writer *writer;
    x3pio_md5 hash;
} main_sink;

static x3pio_status emit_main(void *user, const void *data, size_t size,
                              const char **message)
{
    main_sink *sink = user;
    x3pio_md5_update(&sink->hash, data, size);
    return x3pio_zip_writer_write(sink->writer->zip, data, size, message);
}

x3pio_status x3pio_writer_finish(x3pio_writer *writer)
{
    x3pio_document *out = writer->document;
    x3pio_status status = X3PIO_OK;
    const x3pio_allocator *a = &writer->allocator;
    if (writer->storage == X3PIO_STORAGE_BINARY)
    {
        if (writer->data_started)
        {
            status = x3pio_zip_writer_end(writer->zip, &writer->message);
            writer->data_started = false;
        }
        char digest[X3PIO_MD5_HEX_SIZE];
        x3pio_md5_hex(&writer->hash, digest);
        out->binary = true;
        bool ok = x3pio_document_assign(out, &out->point_path, X3PIO_DATA_FILE,
                                        strlen(X3PIO_DATA_FILE))
                  && x3pio_document_assign(out, &out->point_md5, digest, 32);
        /* The file of validity is needed for integers, which have no NaN. */
        bool integer = false;
        for (size_t k = 0; k < 3; ++k)
        {
            integer = integer
                      || (out->axes[k].type == X3PIO_AXIS_ABSOLUTE
                          && (out->axes[k].storage == X3PIO_TYPE_INT16
                              || out->axes[k].storage == X3PIO_TYPE_INT32));
        }
        if (status == X3PIO_OK && ok && writer->missing_seen && integer
            && writer->bits != NULL)
        {
            const size_t bytes = (writer->count + 7) / 8;
            /* The bits behind the last point are not part of the data. */
            if (writer->count % 8 != 0)
            {
                writer->bits[bytes - 1] &=
                    (unsigned char)((1U << (writer->count % 8)) - 1U);
            }
            char valid_digest[X3PIO_MD5_HEX_SIZE];
            x3pio_md5_of(writer->bits, bytes, valid_digest);
            ok = x3pio_document_assign(out, &out->valid_path, X3PIO_VALID_FILE,
                                       strlen(X3PIO_VALID_FILE))
                 && x3pio_document_assign(out, &out->valid_md5, valid_digest,
                                          32);
            if (ok)
            {
                status = put_member(writer, X3PIO_VALID_FILE, writer->bits,
                                    bytes, &writer->compression);
            }
        }
        if (!ok && status == X3PIO_OK)
        {
            status =
                x3pio_fail(&writer->message, X3PIO_ERR_MEMORY, "Out of memory");
        }
    }
    else
    {
        out->text_values = writer->text_values;
        out->text_value_count =
            writer->count * x3pio_document_absolute_count(out);
        out->text_valid = writer->missing_seen ? writer->text_valid : NULL;
        if (!writer->missing_seen)
        {
            x3pio_release(a, writer->text_valid);
        }
        /* The document owns the numbers now. */
        writer->text_values = NULL;
        writer->text_valid = NULL;
    }
    if (status != X3PIO_OK)
    {
        return status;
    }
    for (size_t i = 0; i < writer->vendor_id_count; ++i)
    {
        if (!x3pio_document_add_vendor(out, writer->vendor_ids[i],
                                       strlen(writer->vendor_ids[i])))
        {
            return x3pio_fail(&writer->message, X3PIO_ERR_MEMORY,
                              "Out of memory");
        }
    }
    if (!x3pio_document_assign(out, &out->checksum_file, X3PIO_CHECKSUM_FILE,
                               strlen(X3PIO_CHECKSUM_FILE)))
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_MEMORY, "Out of memory");
    }

    main_sink sink = {writer, {{0}, {0}, 0}};
    x3pio_md5_init(&sink.hash);
    status = x3pio_zip_writer_begin(writer->zip, X3PIO_MAIN_XML,
                                    &writer->compression, &writer->message);
    if (status == X3PIO_OK)
    {
        status = x3pio_xml_build(out, emit_main, &sink, &writer->message);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_zip_writer_end(writer->zip, &writer->message);
    }
    if (status != X3PIO_OK)
    {
        return status;
    }
    char digest[X3PIO_MD5_HEX_SIZE];
    x3pio_md5_hex(&sink.hash, digest);
    char checksum[X3PIO_MD5_HEX_SIZE + 16];
    const int length =
        snprintf(checksum, sizeof checksum, "%s *%s\n", digest, X3PIO_MAIN_XML);
    status = put_member(writer, X3PIO_CHECKSUM_FILE, checksum,
                        length > 0 ? (size_t)length : 0, &writer->compression);
    for (size_t i = 0; status == X3PIO_OK && i < writer->vendor_file_count; ++i)
    {
        const x3pio_vendor_entry *entry = &writer->vendor_files[i];
        status = put_member(writer, entry->name, entry->content, entry->size,
                            &entry->compression);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_zip_writer_close(writer->zip, &writer->message);
    }
    return status;
}

x3pio_status x3pio_end_file(x3pio_writer *writer)
{
    if (writer == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (!writer->created || writer->ended || writer->failed)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_STATE,
                          "No file is created, the file is ended already, or "
                          "an earlier error ended the writing");
    }
    if (writer->written != writer->count)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_STATE,
                          "Not all points of the file were written");
    }
    x3pio_status status = x3pio_writer_finish(writer);
    if (status != X3PIO_OK)
    {
        writer->failed = true;
        return status;
    }
    writer->ended = true;
    return X3PIO_OK;
}
