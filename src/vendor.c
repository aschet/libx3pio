/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Names of the members of an x3p container.
 */

#include "vendor.h"
#include "allocator.h"
#include "text.h"

/* A URI scheme or the drive letter of a Windows path: a letter, then letters,
 * digits and "+-.", then ':'. */
static bool starts_with_scheme(const char *text, size_t size)
{
    if (size == 0 || !x3pio_is_alpha(text[0]))
    {
        return false;
    }
    for (size_t i = 0; i < size; ++i)
    {
        const char c = text[i];
        if (c == ':')
        {
            return true;
        }
        if (!x3pio_is_alpha(c) && !x3pio_is_digit(c) && c != '+' && c != '-'
            && c != '.')
        {
            return false;
        }
    }
    return false;
}

x3pio_status x3pio_member_path(const x3pio_allocator *allocator,
                               const char *link, size_t size, char **path)
{
    *path = NULL;
    x3pio_trim(&link, &size);
    bool escapes = size == 0 || link[0] == '\\' || link[0] == '/'
                   || starts_with_scheme(link, size);
    char *out = x3pio_allocate(allocator, size + 1);
    if (out == NULL)
    {
        return X3PIO_ERR_MEMORY;
    }
    size_t used = 0;
    size_t begin = 0;
    while (begin <= size)
    {
        size_t end = begin;
        while (end < size && link[end] != '/' && link[end] != '\\')
        {
            ++end;
        }
        const size_t length = end - begin;
        if (length == 0 || (length == 1 && link[begin] == '.'))
        {
            /* An empty segment and "." are dropped. */
        }
        else if (length == 2 && link[begin] == '.' && link[begin + 1] == '.')
        {
            escapes = true;
        }
        else
        {
            if (used > 0)
            {
                out[used++] = '/';
            }
            memcpy(out + used, link + begin, length);
            used += length;
        }
        begin = end + 1;
    }
    out[used] = '\0';
    if (escapes || used == 0)
    {
        x3pio_release(allocator, out);
        return X3PIO_ERR_FORMAT;
    }
    *path = out;
    return X3PIO_OK;
}

bool x3pio_is_reserved_name(const char *path)
{
    return strcmp(path, X3PIO_MAIN_XML) == 0
           || strcmp(path, X3PIO_CHECKSUM_FILE) == 0
           || strcmp(path, X3PIO_DATA_FILE) == 0
           || strcmp(path, X3PIO_VALID_FILE) == 0;
}

/* Tells whether a segment is a name of a file or a folder: not empty, and not
 * "." or "..". */
static bool segment_valid(const char *text, size_t size)
{
    return size > 0 && !(size == 1 && text[0] == '.')
           && !(size == 2 && text[0] == '.' && text[1] == '.');
}

/* The part of an identifier after its scheme, trimmed. */
static void after_scheme(const char **uri, size_t *size, const char **rest,
                         size_t *rest_size)
{
    x3pio_trim(uri, size);
    *rest = *uri;
    *rest_size = *size;
    for (size_t i = 0; i + 2 < *size; ++i)
    {
        if ((*uri)[i] == ':' && (*uri)[i + 1] == '/' && (*uri)[i + 2] == '/')
        {
            *rest = *uri + i + 3;
            *rest_size = *size - (i + 3);
            return;
        }
    }
    if (*size >= 2 && (*uri)[0] == '/' && (*uri)[1] == '/')
    {
        *rest = *uri + 2;
        *rest_size = *size - 2;
    }
}

bool x3pio_vendor_host_is_plain(const char *uri, size_t size)
{
    const char *rest = NULL;
    size_t rest_size = 0;
    after_scheme(&uri, &size, &rest, &rest_size);
    for (size_t i = 0; i < rest_size && rest[i] != '/'; ++i)
    {
        if (rest[i] == ':' || rest[i] == '@')
        {
            return false;
        }
    }
    return true;
}

x3pio_status x3pio_vendor_member(const x3pio_allocator *allocator,
                                 const char *uri, size_t size, char **member)
{
    *member = NULL;
    const char *rest = NULL;
    size_t rest_size = 0;
    after_scheme(&uri, &size, &rest, &rest_size);
    for (size_t i = 0; i < size; ++i)
    {
        if (uri[i] == '\\' || uri[i] == '?' || uri[i] == '#')
        {
            return X3PIO_ERR_ARGUMENT;
        }
    }
    size_t host_size = 0;
    while (host_size < rest_size && rest[host_size] != '/')
    {
        ++host_size;
    }
    if (host_size == 0 || host_size == rest_size)
    {
        return X3PIO_ERR_ARGUMENT;
    }
    const char *path = rest + host_size + 1;
    const size_t path_size = rest_size - host_size - 1;
    char *out = x3pio_allocate(allocator, size + 1);
    if (out == NULL)
    {
        return X3PIO_ERR_MEMORY;
    }
    size_t used = 0;
    /* The labels of the host name. */
    size_t begin = 0;
    bool ok = path_size > 0;
    while (ok && begin <= host_size)
    {
        size_t end = begin;
        while (end < host_size && rest[end] != '.')
        {
            ++end;
        }
        if (end == begin)
        {
            ok = false;
            break;
        }
        if (used > 0)
        {
            out[used++] = '\\';
        }
        memcpy(out + used, rest + begin, end - begin);
        used += end - begin;
        begin = end + 1;
    }
    /* The segments of the path. */
    begin = 0;
    while (ok && begin <= path_size)
    {
        size_t end = begin;
        while (end < path_size && path[end] != '/')
        {
            ++end;
        }
        if (!segment_valid(path + begin, end - begin))
        {
            ok = false;
            break;
        }
        out[used++] = '\\';
        memcpy(out + used, path + begin, end - begin);
        used += end - begin;
        begin = end + 1;
    }
    if (!ok)
    {
        x3pio_release(allocator, out);
        return X3PIO_ERR_ARGUMENT;
    }
    out[used] = '\0';
    *member = out;
    return X3PIO_OK;
}
