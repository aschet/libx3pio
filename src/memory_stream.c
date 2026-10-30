/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief A seekable stream in memory for minizip-ng.
 */

#include "memory_stream.h"
#include "minizip.h"

/* The object that minizip-ng works on. The stream it is made of comes first, as
 * the interface hands out pointers to it. */
typedef struct state
{
    mz_stream base;
    const unsigned char *read_data;
    size_t read_size;
    x3pio_buffer *out;
    size_t position;
    bool opened;
} state;

static size_t state_size(const state *s)
{
    return s->out != NULL ? s->out->size : s->read_size;
}

static const unsigned char *state_data(const state *s)
{
    return s->out != NULL ? s->out->data : s->read_data;
}

static mz_stream_vtbl vtable;

static int32_t stream_open(void *stream, const char *path, int32_t mode)
{
    (void)path;
    state *s = stream;
    s->opened = true;
    s->position = 0;
    if (s->out != NULL && (mode & MZ_OPEN_MODE_CREATE) != 0)
    {
        s->out->size = 0;
    }
    return MZ_OK;
}

static int32_t stream_is_open(void *stream)
{
    return ((state *)stream)->opened ? MZ_OK : MZ_OPEN_ERROR;
}

static int32_t stream_read(void *stream, void *buffer, int32_t size)
{
    state *s = stream;
    if (size < 0 || s->position > state_size(s))
    {
        return MZ_PARAM_ERROR;
    }
    const size_t available = state_size(s) - s->position;
    const size_t count = available < (size_t)size ? available : (size_t)size;
    if (count != 0)
    {
        memcpy(buffer, state_data(s) + s->position, count);
    }
    s->position += count;
    return (int32_t)count;
}

static int32_t stream_write(void *stream, const void *buffer, int32_t size)
{
    state *s = stream;
    if (s->out == NULL || size < 0 || s->position > s->out->size)
    {
        return MZ_PARAM_ERROR;
    }
    const size_t count = (size_t)size;
    size_t end = 0;
    if (!x3pio_add(s->position, count, &end))
    {
        return MZ_MEM_ERROR;
    }
    if (end > s->out->size)
    {
        if (!x3pio_buffer_reserve(s->out, end - s->out->size))
        {
            return MZ_MEM_ERROR;
        }
        /* A gap that a seek past the end made reads as zeros. */
        if (s->position > s->out->size)
        {
            memset(s->out->data + s->out->size, 0, s->position - s->out->size);
        }
        s->out->size = end;
    }
    if (count != 0)
    {
        memcpy(s->out->data + s->position, buffer, count);
    }
    s->position = end;
    return size;
}

static int64_t stream_tell(void *stream)
{
    return (int64_t)((state *)stream)->position;
}

static int32_t stream_seek(void *stream, int64_t offset, int32_t origin)
{
    state *s = stream;
    int64_t base = 0;
    switch (origin)
    {
    case MZ_SEEK_SET:
        break;
    case MZ_SEEK_CUR:
        base = (int64_t)s->position;
        break;
    case MZ_SEEK_END:
        base = (int64_t)state_size(s);
        break;
    default:
        return MZ_PARAM_ERROR;
    }
    const int64_t target = base + offset;
    if (target < 0 || (uint64_t)target > (uint64_t)state_size(s))
    {
        return MZ_SEEK_ERROR;
    }
    s->position = (size_t)target;
    return MZ_OK;
}

static int32_t stream_close(void *stream)
{
    ((state *)stream)->opened = false;
    return MZ_OK;
}

static int32_t stream_error(void *stream)
{
    (void)stream;
    return MZ_OK;
}

/* The signature is the one that minizip-ng calls. */
// NOLINTNEXTLINE(readability-non-const-parameter)
static int32_t stream_get_prop(void *stream, int32_t prop, int64_t *value)
{
    (void)stream;
    (void)prop;
    (void)value;
    return MZ_EXIST_ERROR;
}

static int32_t stream_set_prop(void *stream, int32_t prop, int64_t value)
{
    (void)stream;
    (void)prop;
    (void)value;
    return MZ_EXIST_ERROR;
}

static void *stream_create(void)
{
    state *s = calloc(1, sizeof *s);
    if (s != NULL)
    {
        s->base.vtbl = &vtable;
    }
    return s;
}

static void stream_destroy(void **stream)
{
    if (stream != NULL && *stream != NULL)
    {
        free(*stream);
        *stream = NULL;
    }
}

static mz_stream_vtbl vtable = {
    stream_open,   stream_is_open, stream_read,     stream_write,
    stream_tell,   stream_seek,    stream_close,    stream_error,
    stream_create, stream_destroy, stream_get_prop, stream_set_prop};

static void *make_open_stream(int32_t mode, const void *read_data,
                              size_t read_size, x3pio_buffer *out)
{
    void *stream = mz_stream_create(&vtable);
    if (stream == NULL)
    {
        return NULL;
    }
    state *s = stream;
    s->read_data = read_data;
    s->read_size = read_size;
    s->out = out;
    if (mz_stream_open(stream, NULL, mode) != MZ_OK)
    {
        mz_stream_delete(&stream);
        return NULL;
    }
    return stream;
}

void *x3pio_memory_stream_reader(const void *data, size_t size)
{
    return make_open_stream(MZ_OPEN_MODE_READ, data, size, NULL);
}

void *x3pio_memory_stream_writer(x3pio_buffer *out)
{
    return make_open_stream(MZ_OPEN_MODE_WRITE | MZ_OPEN_MODE_CREATE, NULL, 0,
                            out);
}

void x3pio_memory_stream_free(void *stream)
{
    if (stream != NULL)
    {
        mz_stream_close(stream);
        mz_stream_delete(&stream);
    }
}
