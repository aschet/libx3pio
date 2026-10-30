/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Reading of a zip file with minizip-ng.
 */

#include "allocator.h"
#include "memory_stream.h"
#include "minizip.h"
#include "text.h"
#include "zip.h"

/* A reader of minizip-ng for each member that is open, as a reader walks
 * through the members of its zip file one at a time. */
typedef struct handle
{
    void *reader;
    /* The stream of a zip file in memory, which has to outlive the reader. */
    void *stream;
} handle;

static void handle_close(handle *h)
{
    if (h->reader != NULL)
    {
        mz_zip_reader_close(h->reader);
        mz_zip_reader_delete(&h->reader);
    }
    x3pio_memory_stream_free(h->stream);
    *h = (handle){0};
}

/* Opens the zip file in a reader of minizip-ng. */
static x3pio_status handle_open(handle *h, const x3pio_zip_source *source,
                                const char **message)
{
    *h = (handle){0};
    h->reader = mz_zip_reader_create();
    if (h->reader == NULL)
    {
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    int32_t code = MZ_OK;
    if (source->path != NULL)
    {
        if (mz_os_file_exists(source->path) != MZ_OK
            || mz_os_is_dir(source->path) == MZ_OK)
        {
            handle_close(h);
            return x3pio_fail(message, X3PIO_ERR_IO,
                              "The file cannot be opened");
        }
        code = mz_zip_reader_open_file(h->reader, source->path);
    }
    else
    {
        h->stream = x3pio_memory_stream_reader(source->data, source->size);
        if (h->stream == NULL)
        {
            handle_close(h);
            return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
        }
        code = mz_zip_reader_open(h->reader, h->stream);
    }
    if (code != MZ_OK)
    {
        handle_close(h);
        /* The file was readable, so what minizip-ng refuses is no zip file. */
        return x3pio_fail(message, X3PIO_ERR_FORMAT,
                          "The data is not a readable zip file");
    }
    return X3PIO_OK;
}

void x3pio_zip_names_free(const x3pio_allocator *allocator, char **names,
                          size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        x3pio_release(allocator, names[i]);
    }
    x3pio_release_strings(allocator, names);
}

x3pio_status x3pio_zip_names(const x3pio_zip_source *source,
                             const x3pio_allocator *allocator, char ***names,
                             size_t *count, const char **message)
{
    *names = NULL;
    *count = 0;
    handle h;
    x3pio_status status = handle_open(&h, source, message);
    if (status != X3PIO_OK)
    {
        return status;
    }
    size_t capacity = 16;
    char **list = x3pio_allocate_strings(allocator, capacity);
    if (list == NULL)
    {
        handle_close(&h);
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    size_t used = 0;
    for (int32_t code = mz_zip_reader_goto_first_entry(h.reader); code == MZ_OK;
         code = mz_zip_reader_goto_next_entry(h.reader))
    {
        mz_zip_file *info = NULL;
        if (mz_zip_reader_entry_get_info(h.reader, &info) != MZ_OK
            || info->filename == NULL
            || mz_zip_reader_entry_is_dir(h.reader) == MZ_OK)
        {
            continue;
        }
        if (used == capacity)
        {
            char **grown = x3pio_allocate_strings(allocator, capacity * 2);
            if (grown == NULL)
            {
                status = X3PIO_ERR_MEMORY;
                break;
            }
            x3pio_copy_strings(grown, list, used);
            x3pio_release_strings(allocator, list);
            list = grown;
            capacity *= 2;
        }
        list[used] = x3pio_strdup(allocator, info->filename);
        if (list[used] == NULL)
        {
            status = X3PIO_ERR_MEMORY;
            break;
        }
        ++used;
    }
    handle_close(&h);
    if (status != X3PIO_OK)
    {
        x3pio_zip_names_free(allocator, list, used);
        return x3pio_fail(message, status, "Out of memory");
    }
    *names = list;
    *count = used;
    return X3PIO_OK;
}

struct x3pio_zip_entry
{
    x3pio_allocator allocator;
    handle h;
    uint64_t size;
    bool opened;
};

x3pio_status x3pio_zip_entry_open(const x3pio_zip_source *source,
                                  const x3pio_allocator *allocator,
                                  const char *stored_name,
                                  x3pio_zip_entry **entry, const char **message)
{
    *entry = NULL;
    x3pio_zip_entry *e = x3pio_allocate(allocator, sizeof *e);
    if (e == NULL)
    {
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    *e = (x3pio_zip_entry){.allocator = *allocator};
    x3pio_status status = handle_open(&e->h, source, message);
    if (status != X3PIO_OK)
    {
        x3pio_release(allocator, e);
        return status;
    }
    mz_zip_file *info = NULL;
    if (mz_zip_reader_locate_entry(e->h.reader, stored_name, 0) != MZ_OK
        || mz_zip_reader_entry_get_info(e->h.reader, &info) != MZ_OK)
    {
        handle_close(&e->h);
        x3pio_release(allocator, e);
        return x3pio_fail(message, X3PIO_ERR_IO,
                          "The container has no such member");
    }
    e->size =
        info->uncompressed_size > 0 ? (uint64_t)info->uncompressed_size : 0;
    const int32_t code = mz_zip_reader_entry_open(e->h.reader);
    if (code != MZ_OK)
    {
        handle_close(&e->h);
        x3pio_release(allocator, e);
        const bool unsupported = code == MZ_SUPPORT_ERROR;
        return x3pio_fail(
            message, unsupported ? X3PIO_ERR_UNSUPPORTED : X3PIO_ERR_FORMAT,
            "The member cannot be read");
    }
    e->opened = true;
    *entry = e;
    return X3PIO_OK;
}

uint64_t x3pio_zip_entry_size(const x3pio_zip_entry *entry)
{
    return entry->size;
}

x3pio_status x3pio_zip_entry_read(x3pio_zip_entry *entry, void *buffer,
                                  size_t size, size_t *got,
                                  const char **message)
{
    *got = 0;
    const int32_t want =
        size > ((size_t)1 << 30U) ? (int32_t)(1U << 30U) : (int32_t)size;
    const int32_t count =
        mz_zip_reader_entry_read(entry->h.reader, buffer, want);
    if (count < 0)
    {
        return x3pio_fail(message, X3PIO_ERR_FORMAT,
                          "The zip member is damaged");
    }
    *got = (size_t)count;
    return X3PIO_OK;
}

static void entry_release(x3pio_zip_entry *entry)
{
    handle_close(&entry->h);
    const x3pio_allocator allocator = entry->allocator;
    x3pio_release(&allocator, entry);
}

void x3pio_zip_entry_close(x3pio_zip_entry *entry)
{
    if (entry == NULL)
    {
        return;
    }
    if (entry->opened)
    {
        mz_zip_reader_entry_close(entry->h.reader);
    }
    entry_release(entry);
}

x3pio_status x3pio_zip_entry_finish(x3pio_zip_entry *entry,
                                    const char **message)
{
    if (entry == NULL)
    {
        return X3PIO_OK;
    }
    x3pio_status status = X3PIO_OK;
    unsigned char extra[1];
    const int32_t more = mz_zip_reader_entry_read(entry->h.reader, extra, 1);
    if (more != 0)
    {
        status = x3pio_fail(message, X3PIO_ERR_FORMAT,
                            more < 0 ? "The zip member is damaged"
                                     : "The zip member is longer than the zip "
                                       "file states");
    }
    if (entry->opened)
    {
        /* The checksum of the zip file is checked when the member is closed. */
        const int32_t closed = mz_zip_reader_entry_close(entry->h.reader);
        entry->opened = false;
        if (status == X3PIO_OK && closed != MZ_OK)
        {
            status = x3pio_fail(message, X3PIO_ERR_FORMAT,
                                "The checksum of a zip member is wrong");
        }
    }
    entry_release(entry);
    return status;
}

x3pio_status x3pio_zip_read_member(const x3pio_zip_source *source,
                                   const x3pio_allocator *allocator,
                                   const char *stored_name, size_t limit,
                                   x3pio_buffer *out, const char **message)
{
    x3pio_zip_entry *entry = NULL;
    x3pio_status status =
        x3pio_zip_entry_open(source, allocator, stored_name, &entry, message);
    if (status != X3PIO_OK)
    {
        return status;
    }
    if (entry->size > (uint64_t)limit)
    {
        x3pio_zip_entry_close(entry);
        return x3pio_fail(message, X3PIO_ERR_FORMAT,
                          "The member is larger than the library reads");
    }
    for (;;)
    {
        if (!x3pio_buffer_reserve(out, 16384))
        {
            status = x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
            break;
        }
        size_t got = 0;
        status = x3pio_zip_entry_read(entry, out->data + out->size,
                                      out->capacity - out->size, &got, message);
        if (status != X3PIO_OK || got == 0)
        {
            break;
        }
        out->size += got;
        if (out->size > limit)
        {
            status = x3pio_fail(message, X3PIO_ERR_FORMAT,
                                "The member is larger than the library reads");
            break;
        }
    }
    if (status == X3PIO_OK)
    {
        return x3pio_zip_entry_finish(entry, message);
    }
    x3pio_zip_entry_close(entry);
    return status;
}
