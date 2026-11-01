/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The files of vendors in a container being written.
 */

#include "allocator.h"
#include "text.h"
#include "vendor.h"
#include "writer.h"
#include "xml.h"

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

/* Tells whether one normalized path lies below the other, so that one would be
 * a file and the other a folder. */
static bool lies_below(const char *path, const char *folder)
{
    const size_t size = strlen(folder);
    return strncmp(path, folder, size) == 0 && path[size] == '/';
}

/* Adds a file of a vendor under a name of the container. In Amd 1 an identifier
 * that names a file in the container already, with the same content, shares it;
 * otherwise a name is used once. */
static x3pio_status add_vendor_checked(x3pio_writer *writer, const char *id,
                                       const char *name, const void *content,
                                       size_t size,
                                       const x3pio_compression *compression,
                                       bool amd1)
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
    bool share = false;
    for (size_t i = 0; i < writer->vendor_file_count; ++i)
    {
        const x3pio_vendor_entry *entry = &writer->vendor_files[i];
        char *other = NULL;
        const x3pio_status other_status =
            x3pio_member_path(a, entry->name, strlen(entry->name), &other);
        if (other_status == X3PIO_ERR_MEMORY)
        {
            x3pio_release(a, key);
            return x3pio_fail(&writer->message, other_status, "Out of memory");
        }
        if (other_status != X3PIO_OK)
        {
            continue;
        }
        const bool same = strcmp(other, key) == 0;
        const bool folder = lies_below(other, key) || lies_below(key, other);
        x3pio_release(a, other);
        if (folder)
        {
            x3pio_release(a, key);
            return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                              "The name would be both a file and a folder of "
                              "the container");
        }
        if (same)
        {
            if (amd1 && entry->size == size
                && (size == 0 || memcmp(entry->content, content, size) == 0))
            {
                share = true;
                continue;
            }
            x3pio_release(a, key);
            return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                              "The container has a file of that name already");
        }
    }
    x3pio_release(a, key);

    /* Nothing was refused: commit. */
    const bool known = has_id(writer, id);
    char *id_copy = known ? NULL : x3pio_strdup(a, id);
    char *name_copy = share ? NULL : x3pio_strdup(a, name);
    unsigned char *content_copy = share ? NULL : x3pio_allocate(a, size);
    char **ids = x3pio_allocate_strings(a, writer->vendor_id_count + 1);
    x3pio_vendor_entry *files =
        share ? NULL
              : x3pio_allocate_array(a, writer->vendor_file_count + 1,
                                     sizeof *files);
    if ((!known && id_copy == NULL) || (!share && name_copy == NULL)
        || (!share && content_copy == NULL) || ids == NULL
        || (!share && files == NULL))
    {
        x3pio_release(a, id_copy);
        x3pio_release(a, name_copy);
        x3pio_release(a, content_copy);
        x3pio_release_strings(a, ids);
        x3pio_release(a, files);
        return x3pio_fail(&writer->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    x3pio_copy_strings(ids, writer->vendor_ids, writer->vendor_id_count);
    x3pio_release_strings(a, writer->vendor_ids);
    writer->vendor_ids = ids;
    if (!known)
    {
        writer->vendor_ids[writer->vendor_id_count++] = id_copy;
    }
    if (share)
    {
        return X3PIO_OK;
    }
    if (size > 0)
    {
        memcpy(content_copy, content, size);
    }
    if (writer->vendor_file_count > 0)
    {
        memcpy(files, writer->vendor_files,
               writer->vendor_file_count * sizeof *files);
    }
    x3pio_release(a, writer->vendor_files);
    writer->vendor_files = files;
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
    if (compression != NULL && !x3pio_compression_valid(compression))
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
        if (!x3pio_vendor_host_is_plain(id, strlen(id)))
        {
            return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                              "A vendor id names a domain name without a user "
                              "name or a port");
        }
        /* In Amd 1 the file lies where its identifier says. */
        char *name = NULL;
        x3pio_status status =
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
        if (member != NULL && member[0] != '\0')
        {
            char *given = NULL;
            char *derived = NULL;
            status = x3pio_member_path(&writer->allocator, member,
                                       strlen(member), &given);
            if (status == X3PIO_OK)
            {
                status = x3pio_member_path(&writer->allocator, name,
                                           strlen(name), &derived);
            }
            const bool same = status == X3PIO_OK && strcmp(given, derived) == 0;
            x3pio_release(&writer->allocator, given);
            x3pio_release(&writer->allocator, derived);
            if (!same)
            {
                x3pio_release(&writer->allocator, name);
                return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                                  "In Amd 1 the file lies where its "
                                  "identifier says");
            }
        }
        const x3pio_status added = add_vendor_checked(writer, id, name, content,
                                                      size, compression, true);
        x3pio_release(&writer->allocator, name);
        return added;
    }
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
    return add_vendor_checked(writer, id, member, content, size, compression,
                              false);
}
