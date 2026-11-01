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

/* Adds an identifier without a file. */
static x3pio_status append_id(x3pio_writer *writer, const char *id)
{
    const x3pio_allocator *a = &writer->allocator;
    char *copy = x3pio_strdup(a, id);
    char **ids = x3pio_allocate_strings(a, writer->vendor_id_count + 1);
    if (copy == NULL || ids == NULL)
    {
        x3pio_release(a, copy);
        x3pio_release_strings(a, ids);
        return x3pio_fail(&writer->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    x3pio_copy_strings(ids, writer->vendor_ids, writer->vendor_id_count);
    x3pio_release_strings(a, writer->vendor_ids);
    writer->vendor_ids = ids;
    writer->vendor_ids[writer->vendor_id_count++] = copy;
    return X3PIO_OK;
}

/* The checks every addition shares. */
static x3pio_status check_addition(x3pio_writer *writer, const void *content,
                                   size_t size,
                                   const x3pio_compression *compression)
{
    if (content == NULL && size != 0)
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
    return X3PIO_OK;
}

/* Tells whether a file of the container has the name, which is normalized. */
static bool has_file(x3pio_writer *writer, const char *name, bool *found)
{
    const x3pio_allocator *a = &writer->allocator;
    char *key = NULL;
    if (x3pio_member_path(a, name, strlen(name), &key) != X3PIO_OK)
    {
        return false;
    }
    *found = false;
    for (size_t i = 0; i < writer->vendor_file_count && !*found; ++i)
    {
        char *other = NULL;
        if (x3pio_member_path(a, writer->vendor_files[i].name,
                              strlen(writer->vendor_files[i].name), &other)
            == X3PIO_OK)
        {
            *found = strcmp(other, key) == 0;
            x3pio_release(a, other);
        }
    }
    x3pio_release(a, key);
    return true;
}

/* Amd 1: the identifier names the file, and the file lies where it says. An
 * identifier that was registered without a file takes its file here. */
static x3pio_status add_with_id(x3pio_writer *writer, const char *id,
                                const void *content, size_t size,
                                const x3pio_compression *compression)
{
    if (!x3pio_vendor_host_is_plain(id, strlen(id)))
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "A vendor id names a domain name without a user "
                          "name or a port");
    }
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
                          "A vendor id must be a URI with a host name and a "
                          "path to a file");
    }
    bool named = false;
    if (has_id(writer, id) && has_file(writer, name, &named) && named)
    {
        x3pio_release(&writer->allocator, name);
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "The vendor id is registered already");
    }
    status =
        add_vendor_checked(writer, id, name, content, size, compression, true);
    x3pio_release(&writer->allocator, name);
    return status;
}

/* ISO 25178-72:2017: one identifier for all files, which lie under any name. */
static x3pio_status add_under_vendor(x3pio_writer *writer, const char *vendor,
                                     const char *path, const void *content,
                                     size_t size,
                                     const x3pio_compression *compression)
{
    const char *base = vendor;
    size_t base_size = strlen(vendor);
    x3pio_vendor_base(&base, &base_size);
    if (base_size == 0)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "ISO 25178-72:2017 needs a vendor id");
    }
    if (writer->vendor_id_count > 0
        && !x3pio_vendor_same(writer->vendor_ids[0], vendor))
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "ISO 25178-72:2017 allows one vendor id for all "
                          "files");
    }
    if (writer->vendor_id_count > 0)
    {
        return add_vendor_checked(writer, writer->vendor_ids[0], path, content,
                                  size, compression, false);
    }
    /* The first file fixes the vendor, which is written as it was given. */
    const char *given = vendor;
    size_t given_size = strlen(vendor);
    x3pio_trim(&given, &given_size);
    char *fixed = x3pio_copy_text(&writer->allocator, given, given_size);
    if (fixed == NULL)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    const x3pio_status added = add_vendor_checked(writer, fixed, path, content,
                                                  size, compression, false);
    x3pio_release(&writer->allocator, fixed);
    return added;
}

x3pio_status x3pio_add_vendor_file(x3pio_writer *writer, const char *vendor,
                                   const char *path, const void *content,
                                   size_t size,
                                   const x3pio_compression *compression)
{
    if (writer == NULL || vendor == NULL || path == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    x3pio_status status = check_addition(writer, content, size, compression);
    if (status != X3PIO_OK)
    {
        return status;
    }
    if (writer->document->revision != X3PIO_REVISION_AMD1)
    {
        return add_under_vendor(writer, vendor, path, content, size,
                                compression);
    }
    /* In Amd 1 the identifier of the file is the vendor and the path. */
    char *name = NULL;
    status = x3pio_member_path(&writer->allocator, path, strlen(path), &name);
    if (status == X3PIO_ERR_MEMORY)
    {
        return x3pio_fail(&writer->message, status, "Out of memory");
    }
    if (status != X3PIO_OK)
    {
        return x3pio_fail(
            &writer->message, X3PIO_ERR_ARGUMENT,
            "The path does not point to a member of the container");
    }
    const char *base = vendor;
    size_t base_size = strlen(vendor);
    x3pio_vendor_base(&base, &base_size);
    const size_t name_size = strlen(name);
    char *id =
        x3pio_allocate(&writer->allocator, base_size + 1 + name_size + 1);
    if (id == NULL)
    {
        x3pio_release(&writer->allocator, name);
        return x3pio_fail(&writer->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    (void)snprintf(id, base_size + 1 + name_size + 1, "%.*s/%s", (int)base_size,
                   base, name);
    x3pio_release(&writer->allocator, name);
    status = add_with_id(writer, id, content, size, compression);
    x3pio_release(&writer->allocator, id);
    return status;
}

x3pio_status x3pio_add_vendor_file_by_id(x3pio_writer *writer, const char *id,
                                         const void *content, size_t size,
                                         const x3pio_compression *compression)
{
    if (writer == NULL || id == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    const x3pio_status status =
        check_addition(writer, content, size, compression);
    if (status != X3PIO_OK)
    {
        return status;
    }
    if (writer->document->revision == X3PIO_REVISION_AMD1)
    {
        return add_with_id(writer, id, content, size, compression);
    }
    /* In the first revision the identifier lies below the vendor, which a file
     * has fixed. */
    if (writer->vendor_id_count == 0)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "ISO 25178-72:2017 has one vendor id, which the "
                          "first file fixes: add a file with its vendor first");
    }
    const char *base = writer->vendor_ids[0];
    size_t base_size = strlen(base);
    x3pio_vendor_base(&base, &base_size);
    const char *text = id;
    size_t text_size = strlen(id);
    x3pio_trim(&text, &text_size);
    if (text_size <= base_size + 1 || memcmp(text, base, base_size) != 0
        || text[base_size] != '/')
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "The id does not lie below the vendor of the file; "
                          "ISO 25178-72:2017 has one vendor");
    }
    char *path = x3pio_copy_text(&writer->allocator, text + base_size + 1,
                                 text_size - base_size - 1);
    if (path == NULL)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    const x3pio_status added = add_under_vendor(
        writer, writer->vendor_ids[0], path, content, size, compression);
    x3pio_release(&writer->allocator, path);
    return added;
}

x3pio_status x3pio_add_vendor_id(x3pio_writer *writer, const char *id)
{
    if (writer == NULL || id == NULL)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    const x3pio_status status = check_addition(writer, NULL, 0, NULL);
    if (status != X3PIO_OK)
    {
        return status;
    }
    const char *text = id;
    size_t size = strlen(id);
    x3pio_trim(&text, &size);
    if (size == 0)
    {
        return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                          "A vendor id is not empty");
    }
    if (writer->document->revision == X3PIO_REVISION_AMD1)
    {
        if (has_id(writer, id))
        {
            return x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                              "The vendor id is registered already");
        }
        return append_id(writer, id);
    }
    if (writer->vendor_id_count > 0)
    {
        return x3pio_vendor_same(writer->vendor_ids[0], id)
                   ? X3PIO_OK
                   : x3pio_fail(&writer->message, X3PIO_ERR_ARGUMENT,
                                "ISO 25178-72:2017 allows one vendor id for "
                                "all files");
    }
    return append_id(writer, id);
}
