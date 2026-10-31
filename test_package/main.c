/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Writes a file with the installed library and reads it back. */

#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <stdio.h>

enum
{
    COLUMNS = 4,
    ROWS = 3
};

static int check(x3pio_status status, const char *what)
{
    if (status != X3PIO_OK)
    {
        fprintf(stderr, "%s: %s\n", what, x3pio_status_string(status));
        return 1;
    }
    return 0;
}

int main(void)
{
    static const char path[] = "test_package.x3p";
    float heights[ROWS * COLUMNS];
    float read[ROWS * COLUMNS];
    for (size_t i = 0; i < (size_t)ROWS * COLUMNS; ++i)
    {
        heights[i] = (float)i * 1.0e-6F;
    }

    int failed = 0;
    x3pio_document *document = NULL;
    x3pio_status described = x3pio_document_create(&document);
    if (described == X3PIO_OK)
    {
        described = x3pio_document_set_surface(
            document, COLUMNS, ROWS, 1.0e-6, 1.0e-6, X3PIO_TYPE_FLOAT, 1.0, 1);
    }
    failed |= check(described, "document");
    x3pio_writer *writer = NULL;
    x3pio_status made_writer = x3pio_writer_create(&writer);
    failed |= check(made_writer, "writer");
    if (made_writer == X3PIO_OK && described == X3PIO_OK)
    {
        failed |= check(x3pio_create_file(writer, path, document), "create");
        failed |=
            check(x3pio_write_heights(
                      writer, x3pio_layout_packed(X3PIO_TYPE_FLOAT), heights),
                  "write");
        failed |= check(x3pio_end_file(writer), "end");
    }
    x3pio_writer_free(writer);
    x3pio_document_free(document);

    x3pio_reader *reader = NULL;
    x3pio_status made_reader = x3pio_reader_create(&reader);
    failed |= check(made_reader, "reader");
    if (made_reader == X3PIO_OK)
    {
        failed |= check(x3pio_open_file(reader, path), "open");
        failed |=
            check(x3pio_read_heights(
                      reader, x3pio_layout_packed(X3PIO_TYPE_FLOAT), read),
                  "read");
    }
    x3pio_reader_free(reader);
    remove(path);

    for (size_t i = 0; i < (size_t)ROWS * COLUMNS; ++i)
    {
        failed |= heights[i] != read[i];
    }
    printf("libx3pio %s: %s\n", x3pio_version(), failed ? "failed" : "ok");
    return failed;
}
