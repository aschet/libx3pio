/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Reading main.xml of a container: its document and its checksum.
 */

#include "allocator.h"
#include "reader.h"
#include "text.h"
#include "vendor.h"
#include "xml.h"

/* The bytes of main.xml for the parser, and their checksum. */
typedef struct main_source
{
    x3pio_zip_entry *entry;
    x3pio_md5 hash;
} main_source;

static x3pio_status pull_main(void *user, void *buffer, size_t capacity,
                              size_t *got, const char **message)
{
    main_source *source = user;
    const x3pio_status status =
        x3pio_zip_entry_read(source->entry, buffer, capacity, got, message);
    if (status == X3PIO_OK)
    {
        x3pio_md5_update(&source->hash, buffer, *got);
    }
    return status;
}

/* The digest in a checksum file, which real files lay out in many ways: the
 * bare digest, "digest *main.xml", "digest  main.xml", with a byte order mark
 * or CRLF. Nothing usable means nothing to check. */
static bool digest_of_checksum_file(const unsigned char *content, size_t size,
                                    char digest[X3PIO_MD5_HEX_SIZE])
{
    const char *text = (const char *)content;
    if (size >= 3 && memcmp(text, "\xEF\xBB\xBF", 3) == 0)
    {
        text += 3;
        size -= 3;
    }
    size_t begin = 0;
    while (begin < size && x3pio_is_space(text[begin]))
    {
        ++begin;
    }
    size_t end = begin;
    while (end < size && !x3pio_is_space(text[end]))
    {
        ++end;
    }
    if (end - begin != 32)
    {
        return false;
    }
    for (size_t i = 0; i < 32; ++i)
    {
        if (!x3pio_is_xdigit(text[begin + i]))
        {
            return false;
        }
        digest[i] = x3pio_to_lower(text[begin + i]);
    }
    digest[32] = '\0';
    return true;
}

static x3pio_status verify_main(x3pio_reader *reader, const char *computed)
{
    const x3pio_document *d = reader->document;
    /* A name that is not a link into the container has nothing to check. */
    char *link = NULL;
    const char *name =
        d->checksum_file != NULL ? d->checksum_file : X3PIO_CHECKSUM_FILE;
    x3pio_status status =
        x3pio_member_path(&reader->allocator, name, strlen(name), &link);
    if (status == X3PIO_ERR_MEMORY)
    {
        return x3pio_fail(&reader->message, status, "Out of memory");
    }
    if (status != X3PIO_OK)
    {
        return X3PIO_OK;
    }
    const char *stored = x3pio_member_index_find(&reader->index, link);
    x3pio_release(&reader->allocator, link);
    if (stored == NULL)
    {
        return X3PIO_OK;
    }
    x3pio_buffer content;
    x3pio_buffer_init(&content, &reader->allocator);
    status = x3pio_zip_read_member(&reader->source, &reader->allocator, stored,
                                   X3PIO_MAX_SMALL_MEMBER, &content,
                                   &reader->message);
    if (status != X3PIO_OK)
    {
        x3pio_buffer_free(&content);
        return status;
    }
    char stated[X3PIO_MD5_HEX_SIZE];
    const bool usable =
        digest_of_checksum_file(content.data, content.size, stated);
    x3pio_buffer_free(&content);
    if (usable && !x3pio_md5_hex_equal(stated, computed))
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_CHECKSUM,
                          "The checksum of main.xml does not match the one in "
                          "the checksum file");
    }
    return X3PIO_OK;
}

x3pio_status x3pio_reader_load_document(x3pio_reader *reader)
{
    x3pio_status status = X3PIO_OK;
    main_source main_data = {0};
    status = x3pio_zip_entry_open(&reader->source, &reader->allocator,
                                  reader->index.main_stored, &main_data.entry,
                                  &reader->message);
    if (status != X3PIO_OK)
    {
        return status;
    }
    x3pio_md5_init(&main_data.hash);
    status = x3pio_xml_parse(&reader->allocator, pull_main, &main_data,
                             &reader->document, &reader->message);
    x3pio_zip_entry_close(main_data.entry);
    if (status != X3PIO_OK)
    {
        return status;
    }
    const x3pio_document *d = reader->document;
    if (d->dims.list
        && (d->axes[0].type != X3PIO_AXIS_ABSOLUTE
            || d->axes[1].type != X3PIO_AXIS_ABSOLUTE))
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_FORMAT,
                          "The axes of a point list must be absolute");
    }
    if (reader->verify)
    {
        char computed[X3PIO_MD5_HEX_SIZE];
        x3pio_md5_hex(&main_data.hash, computed);
        status = verify_main(reader, computed);
        if (status != X3PIO_OK)
        {
            return status;
        }
    }
    return X3PIO_OK;
}
