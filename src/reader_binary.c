/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Opening the binary files of the points and of their validity.
 */

#include "allocator.h"
#include "reader.h"
#include "text.h"
#include "vendor.h"
#include "xml.h"

/* Finds the member that a link of main.xml names. */
static x3pio_status find_link(x3pio_reader *reader, const char *link,
                              const char **stored)
{
    char *path = NULL;
    const x3pio_status status =
        x3pio_member_path(&reader->allocator, link, strlen(link), &path);
    if (status == X3PIO_ERR_MEMORY)
    {
        return x3pio_fail(&reader->message, status, "Out of memory");
    }
    if (status != X3PIO_OK)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_FORMAT,
                          "A link does not point to a member of the container");
    }
    *stored = x3pio_member_index_find(&reader->index, path);
    x3pio_release(&reader->allocator, path);
    if (*stored == NULL)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_FORMAT,
                          "The container has no file that a link names");
    }
    return X3PIO_OK;
}

x3pio_status x3pio_reader_open_binary_files(x3pio_reader *reader)
{
    const x3pio_document *d = reader->document;
    const char *point = NULL;
    x3pio_status status = find_link(reader, d->point_path, &point);
    if (status != X3PIO_OK)
    {
        return status;
    }
    size_t want = 0;
    if (!x3pio_multiply(x3pio_document_point_count(d),
                        x3pio_document_record_size(d), &want))
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_FORMAT,
                          "The dimensions of the points are too large");
    }
    x3pio_zip_entry *entry = NULL;
    status = x3pio_zip_entry_open(&reader->source, &reader->allocator, point,
                                  &entry, &reader->message);
    if (status != X3PIO_OK)
    {
        return status;
    }
    const uint64_t have = x3pio_zip_entry_size(entry);
    x3pio_zip_entry_close(entry);
    if (have != (uint64_t)want)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_FORMAT,
                          "The point data file has not the size that the "
                          "dimensions and axes need");
    }
    reader->point_name = x3pio_strdup(&reader->allocator, point);
    if (reader->point_name == NULL)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    if (d->valid_path == NULL || d->valid_path[0] == '\0')
    {
        return X3PIO_OK;
    }
    const char *valid = NULL;
    status = find_link(reader, d->valid_path, &valid);
    if (status != X3PIO_OK)
    {
        return status;
    }
    x3pio_buffer bits;
    x3pio_buffer_init(&bits, &reader->allocator);
    status =
        x3pio_zip_read_member(&reader->source, &reader->allocator, valid,
                              X3PIO_MAX_SMALL_MEMBER, &bits, &reader->message);
    if (status != X3PIO_OK)
    {
        x3pio_buffer_free(&bits);
        return status;
    }
    const size_t bytes = (x3pio_document_point_count(d) + 7) / 8;
    if (bits.size != bytes)
    {
        x3pio_buffer_free(&bits);
        return x3pio_fail(
            &reader->message, X3PIO_ERR_FORMAT,
            "The validity file has not the size that the dimensions need");
    }
    reader->valid_bits = bits.data;
    reader->valid_size = bits.size;
    if (reader->verify && d->valid_md5 != NULL && d->valid_md5[0] != '\0')
    {
        char computed[X3PIO_MD5_HEX_SIZE];
        x3pio_md5_of(reader->valid_bits, reader->valid_size, computed);
        if (!x3pio_md5_hex_equal(d->valid_md5, computed))
        {
            return x3pio_fail(&reader->message, X3PIO_ERR_CHECKSUM,
                              "The checksum of the validity file does not "
                              "match");
        }
    }
    return X3PIO_OK;
}
