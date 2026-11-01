/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* A file whose point data is larger than 4 GiB, which a zip file stores with
 * zip64 records: a binary surface of 40000 by 28000 floats is 4.48 gigabytes
 * of point data. The points are zeros, so the file is a few megabytes on disk.
 * The test takes about 30 seconds and runs only if the environment variable
 * X3PIO_LARGE_TESTS is set; otherwise ctest reports it as skipped. */

#include "helpers.h"

#include <x3pio/file.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* The exit code that ctest treats as a skipped test. */
#define SKIPPED 77

enum
{
    COLUMNS = 40000,
    ROWS = 28000
};

int main(void)
{
    if (getenv("X3PIO_LARGE_TESTS") == NULL)
    {
        printf("skipped: set X3PIO_LARGE_TESTS to run it\n");
        return SKIPPED;
    }
    const char *path = "test_large.x3p";
    float *line = calloc(COLUMNS, sizeof *line);
    float *back = malloc(COLUMNS * sizeof *back);

    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(&doc));
    CHECK_OK(x3pio_document_set_feature_surface(doc, COLUMNS, ROWS, 1e-6, 1e-6,
                                                X3PIO_TYPE_FLOAT, 1.0, 1));
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(x3pio_create_file(writer, path, doc));
    x3pio_const_fields from;
    x3pio_const_fields_init(&from);
    from.z = (x3pio_const_field){line, *x3pio_layout_packed(X3PIO_TYPE_FLOAT)};
    for (size_t row = 0; row < ROWS; ++row)
    {
        CHECK_OK(x3pio_write_points(writer, &from, COLUMNS));
    }
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);
    x3pio_document_free(doc);

    /* The data is far larger than 4 GiB, and the file is small. */
    FILE *file = fopen(path, "rb");
    CHECK(file != NULL);
    if (file != NULL)
    {
        CHECK(fseek(file, 0, SEEK_END) == 0);
        const long size = ftell(file);
        CHECK(size > 0 && size < 64L * 1024L * 1024L);
        fclose(file);
    }

    /* Read back with the checksum of the point data checked after the last
     * row, which covers every byte of it. */
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_file(reader, path));
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.z = (x3pio_field){back, *x3pio_layout_packed(X3PIO_TYPE_FLOAT)};
    size_t wrong = 0;
    for (size_t row = 0; row < ROWS; ++row)
    {
        CHECK_OK(x3pio_read_points(reader, &into, COLUMNS));
        for (size_t i = 0; i < COLUMNS; ++i)
        {
            wrong += back[i] != 0.0F ? 1U : 0U;
        }
    }
    CHECK(wrong == 0);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, &into, COLUMNS));
    x3pio_reader_free(reader);
    remove(path);
    free(line);
    free(back);
    return finish_tests();
}
