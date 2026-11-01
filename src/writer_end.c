/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Ending a file: main.xml, the checksum and the rest of the container.
 */

#include "allocator.h"
#include "text.h"
#include "vendor.h"
#include "writer.h"
#include "xml.h"

/* Writes a member that is in memory. */
static x3pio_status put_member(x3pio_writer *writer, const char *name,
                               const void *data, size_t size,
                               const x3pio_compression *compression)
{
    x3pio_status status = x3pio_zip_writer_begin(writer->zip, name, size,
                                                 compression, &writer->message);
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
                                    x3pio_xml_size_limit(out),
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
