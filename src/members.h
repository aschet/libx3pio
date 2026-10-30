/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The members of a container, found under the names that links use.
 */

#ifndef X3PIO_SRC_MEMBERS_H
#define X3PIO_SRC_MEMBERS_H

#include "common.h"

/** One member of a container. */
typedef struct x3pio_member
{
    /** The name as the zip file stores it. */
    char *stored;
    /** The name as links write it: forward slashes, without a leading ./ . */
    char *name;
} x3pio_member;

/** The members of a container that are files. */
typedef struct x3pio_member_index
{
    /** The allocator of the index. */
    x3pio_allocator allocator;
    /** The members. */
    x3pio_member *members;
    /** The number of members. */
    size_t count;
    /** The folder that holds everything, with its slash, or an empty string. */
    char *prefix;
    /** The stored name of main.xml. */
    const char *main_stored;
} x3pio_member_index;

/**
 * Builds the index of a container from the stored names of its entries. Folders
 * and the files that other systems add (macOS resource forks, Thumbs.db) are
 * left out. A container that has everything in one folder is read as if it had
 * not.
 *
 * @param[out] index The index.
 * @param allocator The allocator.
 * @param[in] names The stored names.
 * @param count The number of names.
 * @param[out] message Receives the reason of a failure.
 * @retval X3PIO_ERR_FORMAT The container has no main.xml, or several folders
 * with one.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
x3pio_status x3pio_member_index_build(x3pio_member_index *index,
                                      const x3pio_allocator *allocator,
                                      char *const *names, size_t count,
                                      const char **message);

/**
 * Releases the index.
 *
 * @param[in,out] index The index.
 */
void x3pio_member_index_free(x3pio_member_index *index);

/**
 * Finds the stored name of a member by the normalized name that a link gives.
 *
 * @param[in] index The index.
 * @param name The normalized name.
 * @return The stored name, or null if the container has no such member.
 */
const char *x3pio_member_index_find(const x3pio_member_index *index,
                                    const char *name);

#endif /* X3PIO_SRC_MEMBERS_H */
