/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The members of a container, found under the names that links use.
 */

#include "members.h"
#include "allocator.h"
#include "text.h"
#include "vendor.h"

/* The last part of a path. */
static const char *base_name(const char *name)
{
    const char *slash = strrchr(name, '/');
    return slash != NULL ? slash + 1 : name;
}

static bool equals_lower(const char *text, size_t size, const char *lower)
{
    return x3pio_equals_nocase(text, size, lower);
}

/* What other operating systems and file managers add to a zip file. */
static bool is_junk(const char *name)
{
    const char *base = base_name(name);
    const size_t base_size = strlen(base);
    if ((base_size >= 2 && base[0] == '.' && base[1] == '_')
        || equals_lower(base, base_size, ".ds_store")
        || equals_lower(base, base_size, "thumbs.db")
        || equals_lower(base, base_size, "desktop.ini"))
    {
        return true;
    }
    const char *segment = name;
    for (;;)
    {
        const char *end = strchr(segment, '/');
        const size_t length =
            end != NULL ? (size_t)(end - segment) : strlen(segment);
        if (equals_lower(segment, length, "__macosx"))
        {
            return true;
        }
        if (end == NULL)
        {
            return false;
        }
        segment = end + 1;
    }
}

/* The name as links write it: slashes, and without leading "./". */
static char *normalized_name(const x3pio_allocator *allocator, const char *raw)
{
    char *name = x3pio_strdup(allocator, raw);
    if (name == NULL)
    {
        return NULL;
    }
    for (char *c = name; *c != '\0'; ++c)
    {
        if (*c == '\\')
        {
            *c = '/';
        }
    }
    const char *start = name;
    while (start[0] == '.' && start[1] == '/')
    {
        start += 2;
    }
    if (start != name)
    {
        memmove(name, start, strlen(start) + 1);
    }
    return name;
}

void x3pio_member_index_free(x3pio_member_index *index)
{
    for (size_t i = 0; i < index->count; ++i)
    {
        x3pio_release(&index->allocator, index->members[i].stored);
        x3pio_release(&index->allocator, index->members[i].name);
    }
    x3pio_release(&index->allocator, index->members);
    x3pio_release(&index->allocator, index->prefix);
    index->members = NULL;
    index->prefix = NULL;
    index->count = 0;
    index->main_stored = NULL;
}

const char *x3pio_member_index_find(const x3pio_member_index *index,
                                    const char *name)
{
    const size_t prefix_size = strlen(index->prefix);
    for (size_t i = 0; i < index->count; ++i)
    {
        const char *candidate = index->members[i].name;
        if (strncmp(candidate, index->prefix, prefix_size) == 0
            && strcmp(candidate + prefix_size, name) == 0)
        {
            return index->members[i].stored;
        }
    }
    return NULL;
}

/* Finds the entry that holds main.xml directly, if any. */
static const char *find_main(const x3pio_member_index *index)
{
    for (size_t i = 0; i < index->count; ++i)
    {
        if (strcmp(index->members[i].name, X3PIO_MAIN_XML) == 0)
        {
            return index->members[i].stored;
        }
    }
    return NULL;
}

x3pio_status x3pio_member_index_build(x3pio_member_index *index,
                                      const x3pio_allocator *allocator,
                                      char *const *names, size_t count,
                                      const char **message)
{
    *index = (x3pio_member_index){.allocator = *allocator};
    index->prefix = x3pio_strdup(allocator, "");
    index->members =
        x3pio_allocate_array(allocator, count, sizeof(x3pio_member));
    if (index->prefix == NULL || index->members == NULL)
    {
        x3pio_member_index_free(index);
        return X3PIO_ERR_MEMORY;
    }
    for (size_t i = 0; i < count; ++i)
    {
        char *name = normalized_name(allocator, names[i]);
        if (name == NULL)
        {
            x3pio_member_index_free(index);
            return X3PIO_ERR_MEMORY;
        }
        const size_t length = strlen(name);
        if (length == 0 || name[length - 1] == '/' || is_junk(name))
        {
            x3pio_release(allocator, name);
            continue;
        }
        char *stored = x3pio_strdup(allocator, names[i]);
        if (stored == NULL)
        {
            x3pio_release(allocator, name);
            x3pio_member_index_free(index);
            return X3PIO_ERR_MEMORY;
        }
        index->members[index->count++] = (x3pio_member){stored, name};
    }
    index->main_stored = find_main(index);
    if (index->main_stored != NULL)
    {
        return X3PIO_OK;
    }
    /* A file zipped by hand has everything in one folder. */
    size_t folders = 0;
    const char *folder_main = NULL;
    const char *folder_name = NULL;
    for (size_t i = 0; i < index->count; ++i)
    {
        const char *name = index->members[i].name;
        const char *slash = strchr(name, '/');
        if (slash != NULL && strchr(slash + 1, '/') == NULL
            && strcmp(slash + 1, X3PIO_MAIN_XML) == 0)
        {
            ++folders;
            folder_main = index->members[i].stored;
            folder_name = name;
        }
    }
    if (folders == 0)
    {
        x3pio_member_index_free(index);
        return x3pio_fail(message, X3PIO_ERR_FORMAT,
                          "The container has no main.xml");
    }
    if (folders > 1)
    {
        x3pio_member_index_free(index);
        return x3pio_fail(message, X3PIO_ERR_FORMAT,
                          "The container has several folders with a main.xml");
    }
    const size_t prefix_size =
        (size_t)(strchr(folder_name, '/') - folder_name) + 1;
    char *prefix = x3pio_copy_text(allocator, folder_name, prefix_size);
    if (prefix == NULL)
    {
        x3pio_member_index_free(index);
        return X3PIO_ERR_MEMORY;
    }
    x3pio_release(allocator, index->prefix);
    index->prefix = prefix;
    index->main_stored = folder_main;
    return X3PIO_OK;
}
