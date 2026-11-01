/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* What the tests share: zip files taken apart and put together with minizip-ng
 * on its own, so that the tests do not trust the zip code of the library, the
 * check of what the writer creates against the schemas of the standard, and
 * builders of files. */

#ifndef X3PIO_TESTS_HELPERS_H
#define X3PIO_TESTS_HELPERS_H

#include "allocator.h"
#include "buffer.h"
#include "check.h"
#include "writer.h"

#include <stdbool.h>
#include <stddef.h>

/* A member of a zip file with its bytes. */
typedef struct member
{
    char name[160];
    x3pio_buffer data;
    /* The method and the level the zip file stores for it. */
    int method;
    int level_flags;
} member;

/* The members of a zip file, as a test changes them. */
typedef struct archive
{
    member *items;
    size_t count;
    size_t capacity;
} archive;

void archive_free(archive *a);
/* Takes a zip file apart; false if minizip-ng cannot read it. */
bool archive_unzip(archive *a, const void *zip, size_t size);
/* Puts the members together again, deflated. */
bool archive_zip(const archive *a, x3pio_buffer *out);
member *archive_find(archive *a, const char *name);
void archive_put(archive *a, const char *name, const void *data, size_t size);
void archive_put_text(archive *a, const char *name, const char *text);
void archive_remove(archive *a, const char *name);
/* Writes the checksum file again, from main.xml. */
void archive_fix_checksum(archive *a);
/* The text of a member, ending with a null character, valid until the member
 * is changed. */
const char *archive_text(archive *a, const char *name);
/* Replaces every occurrence of a text in a member; false if there is none. */
bool archive_replace(archive *a, const char *name, const char *from,
                     const char *to);

/* Replaces the first occurrence of a text in a member; false if there is none.
 */
bool archive_replace_first(archive *a, const char *name, const char *from,
                           const char *to);
/* Removes the text from the first `begin` to the end of the next `end`. */
bool archive_cut(archive *a, const char *name, const char *begin,
                 const char *end);
/* Removes every element of a name, with its content: <name>...</name> and
 * <name/>. */
void archive_strip_element(archive *a, const char *name, const char *tag);

/* Validates a document against a schema of tests/data/xsd. */
bool xsd_validate(const char *schema, const void *xml, size_t size, char *error,
                  size_t capacity);

/* Reads a file of the test data. The bytes are released with free. */
unsigned char *read_test_file(const char *relative, size_t *size);

/* The path of a file of the test data. */
const char *test_path(const char *relative);

/* How a file is created. */
typedef struct file_spec
{
    x3pio_revision revision;
    x3pio_storage storage;
    x3pio_type z_type;
    bool missing;
    bool metadata;
    x3pio_compression compression;
} file_spec;

/* A valid file of 4 x 3 points in memory: the heights 1e-7 * (i + 1), with
 * the point 5 missing if asked. The bytes are in the buffer. */
void create_base_file(const file_spec *spec, x3pio_buffer *out);

/* The same as a set of members. */
void create_base_archive(const file_spec *spec, archive *a);

/* The defaults of a file spec. */
file_spec default_spec(void);

/* Opens a reader on the members; the zip file is kept in the buffer, which
 * has to outlive the reader. */
x3pio_reader *open_archive(const archive *a, x3pio_buffer *keep, bool verify,
                           x3pio_status *status);

/* Reads the heights of a reader in metres; the array is released with free. */
double *read_heights_of(x3pio_reader *reader);

/* Starts a writer in memory on a buffer of the test; the file is in the buffer
 * when the writer has ended it, and the buffer outlives the writer. */
x3pio_status create_in_buffer(x3pio_writer *writer, const x3pio_document *doc,
                              x3pio_buffer *out);

/* Reads a whole file of the disk into a buffer; false if it cannot. */
bool read_whole_file(const char *path, x3pio_buffer *out);

#endif /* X3PIO_TESTS_HELPERS_H */
