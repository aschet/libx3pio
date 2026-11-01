/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The files of vendors in a container: listing them and reading one.
 */

#include "allocator.h"
#include "reader.h"
#include "text.h"
#include "vendor.h"
#include "xml.h"

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
    /* An identifier does not name a file of the format. */
    if (x3pio_is_reserved_name(path)
        || x3pio_member_index_find(&reader->index, path) == NULL)
    {
        x3pio_release(&reader->allocator, path);
        return X3PIO_OK;
    }
    *found = path;
    return X3PIO_OK;
}

x3pio_status x3pio_reader_find_vendor_files(x3pio_reader *reader)
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
    enum
    {
        TAKEN = 7
    };
    char *taken[TAKEN] = {NULL};
    /* The files of the format, also those the document does not use, and the
     * files the document names. */
    const char *names[TAKEN] = {
        X3PIO_MAIN_XML,   X3PIO_CHECKSUM_FILE,
        X3PIO_DATA_FILE,  X3PIO_VALID_FILE,
        d->checksum_file, d->binary ? d->point_path : NULL};
    x3pio_status status = X3PIO_OK;
    for (size_t i = 0; i < TAKEN && status == X3PIO_OK; ++i)
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
        for (size_t t = 0; t < TAKEN; ++t)
        {
            is_taken =
                is_taken || (taken[t] != NULL && strcmp(taken[t], name) == 0);
        }
        if (!is_taken)
        {
            /* The identifier of a file is the vendor and the name, as in Amd 1
             * it is the whole of it. */
            const char *vendor = d->vendor_ids[0];
            size_t vendor_size = strlen(vendor);
            x3pio_vendor_base(&vendor, &vendor_size);
            const size_t name_size = strlen(name);
            char *id = x3pio_allocate(&reader->allocator,
                                      vendor_size + 1 + name_size + 1);
            if (id == NULL)
            {
                status = x3pio_fail(&reader->message, X3PIO_ERR_MEMORY,
                                    "Out of memory");
            }
            else
            {
                (void)snprintf(id, vendor_size + 1 + name_size + 1, "%.*s/%s",
                               (int)vendor_size, vendor, name);
                if (!add_vendor(reader, id, name))
                {
                    status = x3pio_fail(&reader->message, X3PIO_ERR_MEMORY,
                                        "Out of memory");
                }
                x3pio_release(&reader->allocator, id);
            }
        }
    }
    for (size_t t = 0; t < TAKEN; ++t)
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

x3pio_status x3pio_read_vendor_file(x3pio_reader *reader, const char *id,
                                    void *buffer, size_t capacity, size_t *size)
{
    if (reader == NULL || id == NULL || size == NULL
        || (buffer == NULL && capacity != 0))
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (!reader->opened)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_STATE, "No file is open");
    }
    for (size_t i = 0; i < reader->vendor_count; ++i)
    {
        if (strcmp(reader->vendors[i].id, id) != 0)
        {
            continue;
        }
        if (reader->vendors[i].member == NULL)
        {
            return x3pio_fail(&reader->message, X3PIO_ERR_ARGUMENT,
                              "The vendor id names no file of the container");
        }
        return x3pio_read_member(reader, reader->vendors[i].member, buffer,
                                 capacity, size);
    }
    return x3pio_fail(&reader->message, X3PIO_ERR_ARGUMENT,
                      "The file does not register that vendor id");
}

x3pio_status x3pio_reader_find_vendor_file(x3pio_reader *reader,
                                           const char *prefix, size_t first,
                                           size_t *index)
{
    if (reader == NULL || prefix == NULL || index == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    if (!reader->opened)
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_STATE, "No file is open");
    }
    if (!x3pio_vendor_prefix_valid(prefix))
    {
        return x3pio_fail(&reader->message, X3PIO_ERR_ARGUMENT,
                          "The prefix is not a URI of a domain name with an "
                          "optional path");
    }
    *index = reader->vendor_count;
    for (size_t i = first; i < reader->vendor_count; ++i)
    {
        if (x3pio_vendor_below(prefix, reader->vendors[i].id))
        {
            *index = i;
            break;
        }
    }
    return X3PIO_OK;
}
