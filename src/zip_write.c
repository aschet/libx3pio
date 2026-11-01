/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Writing of a zip file with minizip-ng.
 */

#include "allocator.h"
#include "memory_stream.h"
#include "minizip.h"
#include "text.h"
#include "zip.h"

struct x3pio_zip_writer
{
    x3pio_allocator allocator;
    void *writer;
    /* The stream of a zip file in memory, which has to outlive the writer. */
    void *stream;
    /* The buffer of a zip file in memory, or null. */
    x3pio_buffer *out;
    bool entry_open;
};

/* An error of minizip-ng is a memory error if the buffer that it writes into
 * could not grow. */
static x3pio_status failure(const x3pio_zip_writer *writer,
                            const char **message, const char *text)
{
    if (writer->out != NULL && writer->out->failed)
    {
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    return x3pio_fail(message, X3PIO_ERR_IO, text);
}

/* Fixed so that the same content gives the same bytes: noon on 1980-01-02 is
 * the first day of the DOS time stamp in every time zone. */
#define ZIP_TIME ((time_t)315662400)

/* The room that a plain member keeps below 4 GiB for the compression. */
#define ZIP_PLAIN_MARGIN ((uint64_t)16 * 1024 * 1024)

static void configure(void *writer)
{
    mz_zip_writer_set_compress_method(writer, MZ_COMPRESS_METHOD_DEFLATE);
    mz_zip_writer_set_compress_level(writer, MZ_COMPRESS_LEVEL_DEFAULT);
}

static x3pio_zip_writer *create(const x3pio_allocator *allocator)
{
    x3pio_zip_writer *w = x3pio_allocate(allocator, sizeof *w);
    if (w == NULL)
    {
        return NULL;
    }
    *w = (x3pio_zip_writer){.allocator = *allocator};
    w->writer = mz_zip_writer_create();
    if (w->writer == NULL)
    {
        x3pio_release(allocator, w);
        return NULL;
    }
    configure(w->writer);
    return w;
}

void x3pio_zip_writer_free(x3pio_zip_writer *writer)
{
    if (writer == NULL)
    {
        return;
    }
    if (writer->writer != NULL)
    {
        mz_zip_writer_delete(&writer->writer);
    }
    /* The writer is gone, and with it its use of the stream. */
    x3pio_memory_stream_free(writer->stream);
    const x3pio_allocator allocator = writer->allocator;
    x3pio_release(&allocator, writer);
}

x3pio_status x3pio_zip_writer_open_file(const x3pio_allocator *allocator,
                                        const char *path,
                                        x3pio_zip_writer **writer,
                                        const char **message)
{
    *writer = NULL;
    x3pio_zip_writer *w = create(allocator);
    if (w == NULL)
    {
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    /* minizip-ng creates the folders of a path that are missing; a program
     * that names a file in a folder that is not there got it wrong. */
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *last = slash > backslash ? slash : backslash;
    if (last != NULL && last != path)
    {
        char *folder = x3pio_copy_text(allocator, path, (size_t)(last - path));
        const bool exists = folder != NULL && mz_os_is_dir(folder) == MZ_OK;
        x3pio_release(allocator, folder);
        if (!exists)
        {
            x3pio_zip_writer_free(w);
            return x3pio_fail(message, X3PIO_ERR_IO,
                              "The folder of the file does not exist");
        }
    }
    const int32_t code = mz_zip_writer_open_file(w->writer, path, 0, 0);
    if (code != MZ_OK)
    {
        x3pio_zip_writer_free(w);
        return x3pio_fail(message, X3PIO_ERR_IO, "The file cannot be created");
    }
    *writer = w;
    return X3PIO_OK;
}

x3pio_status x3pio_zip_writer_open_memory(const x3pio_allocator *allocator,
                                          x3pio_buffer *out,
                                          x3pio_zip_writer **writer,
                                          const char **message)
{
    *writer = NULL;
    x3pio_zip_writer *w = create(allocator);
    if (w == NULL)
    {
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    w->out = out;
    w->stream = x3pio_memory_stream_writer(out);
    if (w->stream == NULL
        || mz_zip_writer_open(w->writer, w->stream, 0) != MZ_OK)
    {
        x3pio_zip_writer_free(w);
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    *writer = w;
    return X3PIO_OK;
}

x3pio_status x3pio_zip_writer_begin(x3pio_zip_writer *writer, const char *name,
                                    uint64_t size_limit,
                                    const x3pio_compression *compression,
                                    const char **message)
{
    mz_zip_file info;
    memset(&info, 0, sizeof info);
    info.version_madeby = MZ_VERSION_MADEBY;
    info.compression_method = compression->method == X3PIO_COMPRESS_STORE
                                  ? MZ_COMPRESS_METHOD_STORE
                                  : MZ_COMPRESS_METHOD_DEFLATE;
    info.flag = MZ_ZIP_FLAG_UTF8;
    /* Deflate can make high-entropy data larger, by far less than the margin.
     */
    info.zip64 = size_limit > UINT32_MAX - ZIP_PLAIN_MARGIN ? MZ_ZIP64_FORCE
                                                            : MZ_ZIP64_DISABLE;
    info.modified_date = ZIP_TIME;
    info.accessed_date = ZIP_TIME;
    info.creation_date = ZIP_TIME;
    info.filename = name;
    if (MZ_HOST_SYSTEM(info.version_madeby) == MZ_HOST_SYSTEM_UNIX)
    {
        info.external_fa = 0100644U << 16U;
    }
    /* The sizes and the checksum go into the header after the bytes of the
     * member, which every reader accepts, and not behind the bytes. The zip
     * file exists once it is opened. */
    void *zip = NULL;
    if (mz_zip_writer_get_zip_handle(writer->writer, &zip) == MZ_OK)
    {
        mz_zip_set_data_descriptor(zip, 0);
    }
    /* The level is read when the member is opened. */
    mz_zip_writer_set_compress_level(writer->writer,
                                     (int16_t)compression->level);
    const int32_t code = mz_zip_writer_entry_open(writer->writer, &info);
    if (code != MZ_OK || (writer->out != NULL && writer->out->failed))
    {
        return failure(writer, message,
                       "A member cannot be added to the zip file");
    }
    writer->entry_open = true;
    return X3PIO_OK;
}

x3pio_status x3pio_zip_writer_write(x3pio_zip_writer *writer, const void *data,
                                    size_t size, const char **message)
{
    const unsigned char *bytes = data;
    while (size > 0)
    {
        const int32_t block =
            size > ((size_t)1 << 30U) ? (int32_t)(1U << 30U) : (int32_t)size;
        const int32_t put =
            mz_zip_writer_entry_write(writer->writer, bytes, block);
        if (put != block || (writer->out != NULL && writer->out->failed))
        {
            return failure(writer, message, "The zip file cannot be written");
        }
        bytes += block;
        size -= (size_t)block;
    }
    return X3PIO_OK;
}

x3pio_status x3pio_zip_writer_end(x3pio_zip_writer *writer,
                                  const char **message)
{
    writer->entry_open = false;
    const int32_t code = mz_zip_writer_entry_close(writer->writer);
    if (code != MZ_OK || (writer->out != NULL && writer->out->failed))
    {
        return failure(writer, message,
                       "A member of the zip file cannot be completed");
    }
    return X3PIO_OK;
}

x3pio_status x3pio_zip_writer_close(x3pio_zip_writer *writer,
                                    const char **message)
{
    const int32_t code = mz_zip_writer_close(writer->writer);
    if (code != MZ_OK || (writer->out != NULL && writer->out->failed))
    {
        return failure(writer, message, "The zip file cannot be completed");
    }
    return X3PIO_OK;
}
