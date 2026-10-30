/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief A buffer of bytes that grows.
 */

#include "buffer.h"

void x3pio_buffer_init(x3pio_buffer *buffer, const x3pio_allocator *allocator)
{
    *buffer = (x3pio_buffer){.allocator = allocator};
}

void x3pio_buffer_free(x3pio_buffer *buffer)
{
    x3pio_release(buffer->allocator, buffer->data);
    *buffer = (x3pio_buffer){.allocator = buffer->allocator};
}

bool x3pio_buffer_reserve(x3pio_buffer *buffer, size_t extra)
{
    size_t need = 0;
    if (!x3pio_add(buffer->size, extra, &need))
    {
        buffer->failed = true;
        return false;
    }
    if (need <= buffer->capacity)
    {
        return true;
    }
    size_t capacity = buffer->capacity != 0 ? buffer->capacity : 256;
    while (capacity < need)
    {
        if (capacity > SIZE_MAX / 2)
        {
            capacity = need;
            break;
        }
        capacity *= 2;
    }
    unsigned char *grown = x3pio_allocate(buffer->allocator, capacity);
    if (grown == NULL)
    {
        buffer->failed = true;
        return false;
    }
    if (buffer->size > 0)
    {
        memcpy(grown, buffer->data, buffer->size);
    }
    x3pio_release(buffer->allocator, buffer->data);
    buffer->data = grown;
    buffer->capacity = capacity;
    return true;
}

bool x3pio_buffer_append(x3pio_buffer *buffer, const void *data, size_t size)
{
    if (!x3pio_buffer_reserve(buffer, size))
    {
        return false;
    }
    if (size > 0)
    {
        memcpy(buffer->data + buffer->size, data, size);
        buffer->size += size;
    }
    return true;
}

bool x3pio_buffer_append_text(x3pio_buffer *buffer, const char *text)
{
    return x3pio_buffer_append(buffer, text, strlen(text));
}

bool x3pio_buffer_append_char(x3pio_buffer *buffer, char c)
{
    return x3pio_buffer_append(buffer, &c, 1);
}
