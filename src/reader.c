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

/* The largest checksum file or file of validity bits that is read into memory.
 */
#define MAX_SMALL_MEMBER ((size_t)1 << 30U)

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
    x3pio_release(a, reader->owned);
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
    reader->owned = NULL;
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

/* ---- main.xml ------------------------------------------------------------ */

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

static bool md5_equals(const char *stated, const char *computed)
{
    const size_t size = strlen(stated);
    if (size != 32)
    {
        return false;
    }
    for (size_t i = 0; i < size; ++i)
    {
        if (x3pio_to_lower(stated[i]) != computed[i])
        {
            return false;
        }
    }
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
    status =
        x3pio_zip_read_member(&reader->source, &reader->allocator, stored,
                              MAX_SMALL_MEMBER, &content, &reader->message);
    if (status != X3PIO_OK)
    {
        x3pio_buffer_free(&content);
        return status;
    }
    char stated[X3PIO_MD5_HEX_SIZE];
    const bool usable =
        digest_of_checksum_file(content.data, content.size, stated);
    x3pio_buffer_free(&content);
    if (usable && !md5_equals(stated, computed))
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_CHECKSUM,
                          "The checksum of main.xml does not match the one in "
                          "the checksum file");
    }
    return X3PIO_OK;
}

/* ---- the binary files -----------------------------------------------------
 */

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

static x3pio_status open_binary_files(x3pio_reader *reader)
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
    status = x3pio_zip_read_member(&reader->source, &reader->allocator, valid,
                                   MAX_SMALL_MEMBER, &bits, &reader->message);
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
        if (!md5_equals(d->valid_md5, computed))
        {
            return x3pio_fail(&reader->message, X3PIO_ERR_CHECKSUM,
                              "The checksum of the validity file does not "
                              "match");
        }
    }
    return X3PIO_OK;
}

/* ---- the files of vendors --------------------------------------------------
 */

static bool add_vendor(x3pio_reader *reader, const char *id, const char *member)
{
    x3pio_vendor_file *grown = x3pio_allocate_array(
        &reader->allocator, reader->vendor_count + 1, sizeof *grown);
    if (grown == NULL)
    {
        return false;
    }
    char *id_copy = x3pio_strdup(&reader->allocator, id);
    char *member_copy =
        member != NULL ? x3pio_strdup(&reader->allocator, member) : NULL;
    if (id_copy == NULL || (member != NULL && member_copy == NULL))
    {
        x3pio_release(&reader->allocator, id_copy);
        x3pio_release(&reader->allocator, member_copy);
        x3pio_release(&reader->allocator, grown);
        return false;
    }
    if (reader->vendor_count > 0)
    {
        memcpy(grown, reader->vendors, reader->vendor_count * sizeof *grown);
    }
    x3pio_release(&reader->allocator, reader->vendors);
    grown[reader->vendor_count++] = (x3pio_vendor_file){id_copy, member_copy};
    reader->vendors = grown;
    return true;
}

/* The normalized name of a member, if the container has it. */
static x3pio_status existing_member(x3pio_reader *reader, const char *name,
                                    char **found)
{
    *found = NULL;
    char *path = NULL;
    const x3pio_status status =
        x3pio_member_path(&reader->allocator, name, strlen(name), &path);
    if (status == X3PIO_ERR_MEMORY)
    {
        return x3pio_fail(&reader->message, status, "Out of memory");
    }
    if (status != X3PIO_OK)
    {
        return X3PIO_OK;
    }
    if (x3pio_member_index_find(&reader->index, path) == NULL)
    {
        x3pio_release(&reader->allocator, path);
        return X3PIO_OK;
    }
    *found = path;
    return X3PIO_OK;
}

static x3pio_status find_vendor_files(x3pio_reader *reader)
{
    const x3pio_document *d = reader->document;
    if (d->vendor_count == 0)
    {
        return X3PIO_OK;
    }
    if (d->revision == X3PIO_REVISION_AMD1)
    {
        for (size_t i = 0; i < d->vendor_count; ++i)
        {
            /* An id without a path to a file has no file. */
            char *member = NULL;
            char *name = NULL;
            x3pio_status status =
                x3pio_vendor_member(&reader->allocator, d->vendor_ids[i],
                                    strlen(d->vendor_ids[i]), &name);
            if (status == X3PIO_ERR_MEMORY)
            {
                return x3pio_fail(&reader->message, status, "Out of memory");
            }
            if (status == X3PIO_OK)
            {
                status = existing_member(reader, name, &member);
                x3pio_release(&reader->allocator, name);
                if (status != X3PIO_OK)
                {
                    return status;
                }
            }
            const bool added = add_vendor(reader, d->vendor_ids[i], member);
            x3pio_release(&reader->allocator, member);
            if (!added)
            {
                return x3pio_fail(&reader->message, X3PIO_ERR_MEMORY,
                                  "Out of memory");
            }
        }
        return X3PIO_OK;
    }
    /* In the first revision the one vendor owns every file that is not part of
     * the format. */
    char *taken[4] = {NULL, NULL, NULL, NULL};
    const char *names[4] = {
        X3PIO_MAIN_XML,
        d->checksum_file != NULL ? d->checksum_file : X3PIO_CHECKSUM_FILE,
        d->binary ? d->point_path : NULL, d->binary ? d->valid_path : NULL};
    x3pio_status status = X3PIO_OK;
    for (size_t i = 0; i < 4 && status == X3PIO_OK; ++i)
    {
        if (names[i] != NULL
            && x3pio_member_path(&reader->allocator, names[i], strlen(names[i]),
                                 &taken[i])
                   == X3PIO_ERR_MEMORY)
        {
            status =
                x3pio_fail(&reader->message, X3PIO_ERR_MEMORY, "Out of memory");
        }
    }
    const size_t prefix_size = strlen(reader->index.prefix);
    for (size_t i = 0; i < reader->index.count && status == X3PIO_OK; ++i)
    {
        const x3pio_member *member = &reader->index.members[i];
        if (strncmp(member->name, reader->index.prefix, prefix_size) != 0
            || member->stored == reader->index.main_stored)
        {
            continue;
        }
        const char *name = member->name + prefix_size;
        bool is_taken = false;
        for (size_t t = 0; t < 4; ++t)
        {
            is_taken =
                is_taken || (taken[t] != NULL && strcmp(taken[t], name) == 0);
        }
        if (!is_taken && !add_vendor(reader, d->vendor_ids[0], name))
        {
            status =
                x3pio_fail(&reader->message, X3PIO_ERR_MEMORY, "Out of memory");
        }
    }
    for (size_t t = 0; t < 4; ++t)
    {
        x3pio_release(&reader->allocator, taken[t]);
    }
    return status;
}

size_t x3pio_reader_vendor_file_count(const x3pio_reader *reader)
{
    return reader != NULL ? reader->vendor_count : 0;
}

const char *x3pio_reader_vendor_file_id(const x3pio_reader *reader,
                                        size_t index)
{
    if (reader == NULL || index >= reader->vendor_count)
    {
        return NULL;
    }
    return reader->vendors[index].id;
}

const char *x3pio_reader_vendor_file_member(const x3pio_reader *reader,
                                            size_t index)
{
    if (reader == NULL || index >= reader->vendor_count)
    {
        return NULL;
    }
    return reader->vendors[index].member;
}

x3pio_status x3pio_read_member(x3pio_reader *reader, const char *member,
                               void *buffer, size_t capacity, size_t *size)
{
    if (reader == NULL || member == NULL || size == NULL
        || (buffer == NULL && capacity != 0))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (!reader->opened)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_STATE, "No file is open");
    }
    char *path = NULL;
    x3pio_status status =
        x3pio_member_path(&reader->allocator, member, strlen(member), &path);
    if (status == X3PIO_ERR_MEMORY)
    {
        return x3pio_fail(&reader->message, status, "Out of memory");
    }
    if (status != X3PIO_OK)
    {
        return x3pio_fail(
            &reader->message, X3PIO_ERR_ARGUMENT,
            "The name does not point to a member of the container");
    }
    const char *stored = x3pio_member_index_find(&reader->index, path);
    x3pio_release(&reader->allocator, path);
    if (stored == NULL)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_IO,
                          "The container has no such member");
    }
    x3pio_zip_entry *entry = NULL;
    status = x3pio_zip_entry_open(&reader->source, &reader->allocator, stored,
                                  &entry, &reader->message);
    if (status != X3PIO_OK)
    {
        return status;
    }
    const uint64_t have = x3pio_zip_entry_size(entry);
    *size = (size_t)have;
    if (have > (uint64_t)SIZE_MAX || (size_t)have > capacity)
    {
        x3pio_zip_entry_close(entry);
        return x3pio_fail(&reader->message, X3PIO_ERR_RANGE,
                          "The member does not fit the buffer");
    }
    size_t done = 0;
    while (status == X3PIO_OK)
    {
        size_t got = 0;
        unsigned char scratch[1];
        /* A read into the room that is left; one byte more shows a member that
         * is larger than the zip file stated. */
        status = x3pio_zip_entry_read(
            entry, done < capacity ? (unsigned char *)buffer + done : scratch,
            done < capacity ? capacity - done : sizeof scratch, &got,
            &reader->message);
        if (status != X3PIO_OK || got == 0)
        {
            break;
        }
        if (done >= capacity)
        {
            status = x3pio_fail(&reader->message, X3PIO_ERR_FORMAT,
                                "The member is larger than the zip file "
                                "states");
            break;
        }
        done += got;
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_zip_entry_finish(entry, &reader->message);
    }
    else
    {
        x3pio_zip_entry_close(entry);
    }
    if (status == X3PIO_OK && done != *size)
    {
        status = x3pio_fail(&reader->message, X3PIO_ERR_FORMAT,
                            "The member is smaller than the zip file states");
    }
    return status;
}

/* ---- opening ----------------------------------------------------------------
 */

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
    if (d->binary)
    {
        status = open_binary_files(reader);
        if (status != X3PIO_OK)
        {
            return status;
        }
    }
    return find_vendor_files(reader);
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

x3pio_status x3pio_open_stream(x3pio_reader *reader, const x3pio_stream *stream)
{
    if (reader == NULL || stream == NULL || stream->read == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    const x3pio_status ready = require_closed(reader);
    if (ready != X3PIO_OK)
    {
        return ready;
    }
    x3pio_buffer all;
    x3pio_buffer_init(&all, &reader->allocator);
    for (;;)
    {
        if (!x3pio_buffer_reserve(&all, 65536))
        {
            x3pio_buffer_free(&all);
            return x3pio_fail(&reader->message, X3PIO_ERR_MEMORY,
                              "Out of memory");
        }
        const size_t got = stream->read(stream->user, all.data + all.size,
                                        all.capacity - all.size);
        if (got == X3PIO_STREAM_ERROR || got > all.capacity - all.size)
        {
            x3pio_buffer_free(&all);
            return x3pio_fail(&reader->message, X3PIO_ERR_IO,
                              "The stream could not be read");
        }
        if (got == 0)
        {
            break;
        }
        all.size += got;
    }
    reader->owned = all.data;
    reader->source = (x3pio_zip_source){NULL, all.data, all.size};
    return open_and_keep(reader);
}
