/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The public interface only, through the library that is installed: no header
 * and no function of the implementation. A program that uses the library like
 * this does not need the headers of the libraries that the implementation uses.
 */

#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect(bool condition, const char *what)
{
    if (!condition)
    {
        fprintf(stderr, "failed: %s\n", what);
        ++failures;
    }
}

int main(void)
{
    expect(strcmp(x3pio_version(), X3PIO_VERSION_STRING) == 0,
           "the header and the library agree on the version");
    expect(strcmp(x3pio_status_string(X3PIO_OK), "no error") == 0, "status");

    /* Write a file of integers with a mask, and read it back. */
    static const char path[] = "test_public.x3p";
    x3pio_document *document = NULL;
    expect(x3pio_document_create(&document) == X3PIO_OK, "document");
    expect(x3pio_document_describe_surface(document, 4, 3, 1e-6, 1e-6,
                                           X3PIO_TYPE_INT16,
                                           X3PIO_AUTO_INCREMENT, 1)
               == X3PIO_OK,
           "surface");
    expect(x3pio_document_set_software_metadata(document, "test_public", "1")
               == X3PIO_OK,
           "metadata");
    double heights[12];
    for (size_t i = 0; i < 12; ++i)
    {
        heights[i] = 1e-7 * (double)i;
    }
    heights[5] = NAN;
    x3pio_writer *writer = NULL;
    expect(x3pio_writer_create(&writer) == X3PIO_OK, "writer");
    expect(x3pio_create_file(writer, path, document) == X3PIO_OK, "create");
    expect(x3pio_write_heights(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE),
                               heights)
               == X3PIO_OK,
           "write");
    expect(x3pio_end_file(writer) == X3PIO_OK, "end");
    x3pio_writer_free(writer);
    x3pio_document_free(document);

    x3pio_reader *reader = NULL;
    expect(x3pio_reader_create(&reader) == X3PIO_OK, "reader");
    expect(x3pio_open_file(reader, path) == X3PIO_OK, "open");
    const x3pio_document *read = x3pio_reader_document(reader);
    expect(read != NULL && x3pio_document_has_validity(read),
           "the file has a mask");
    expect(read != NULL && x3pio_document_columns(read) == 4
               && x3pio_document_rows(read) == 3,
           "the size");
    int16_t counts[12];
    unsigned char mask[12];
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.z = (x3pio_field){counts, *x3pio_layout_packed(X3PIO_TYPE_INT16)};
    into.valid = (x3pio_field){mask, *x3pio_layout_packed(X3PIO_TYPE_UINT8)};
    expect(x3pio_read_data(reader, &into, 3) == X3PIO_OK, "read");
    expect(mask[5] == 0 && mask[4] == 1, "the mask");
    double metres[12];
    expect(x3pio_decode_values(read, X3PIO_Z, counts, 12,
                               x3pio_layout_packed(X3PIO_TYPE_DOUBLE), metres)
               == X3PIO_OK,
           "decode");
    expect(fabs(metres[11] - heights[11]) < 1e-9, "the heights");
    x3pio_reader_free(reader);
    remove(path);

    if (failures == 0)
    {
        printf("libx3pio %s: ok\n", x3pio_version());
    }
    return failures == 0 ? 0 : 1;
}
