#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Reads a file from standard input, for example from a pipe:
 *
 *     cat surface.x3p | read_stream
 *
 * A zip file lists its members at its end, so a reader needs the whole file
 * before it can read any of it. The program collects the bytes in a buffer
 * first, and opens that. The rows are then read in pieces, here a row at a
 * time, so the program needs the memory of the zip file and of one row of
 * metres. */
static unsigned char *read_all(FILE *input, size_t *size)
{
    size_t capacity = 65536;
    size_t used = 0;
    unsigned char *bytes = malloc(capacity);
    while (bytes != NULL)
    {
        if (used == capacity)
        {
            capacity *= 2;
            unsigned char *grown = realloc(bytes, capacity);
            if (grown == NULL)
            {
                free(bytes);
                return NULL;
            }
            bytes = grown;
        }
        const size_t got = fread(bytes + used, 1, capacity - used, input);
        if (got == 0)
        {
            break;
        }
        used += got;
    }
    *size = used;
    return bytes;
}

int main(void)
{
    size_t size = 0;
    unsigned char *file = read_all(stdin, &size);
    if (file == NULL)
    {
        return EXIT_FAILURE;
    }
    x3pio_reader *reader = NULL;
    x3pio_reader_create(&reader);
    x3pio_status status = x3pio_open_memory(reader, file, size);
    if (status != X3PIO_OK)
    {
        fprintf(stderr, "%s\n", x3pio_reader_error_message(reader));
        x3pio_reader_free(reader);
        free(file);
        return EXIT_FAILURE;
    }
    const x3pio_document *document = x3pio_reader_document(reader);
    const size_t columns = x3pio_document_columns(document);
    const size_t rows = x3pio_document_rows(document);
    double *row = malloc(columns * sizeof *row);
    double lowest = INFINITY;
    double highest = -INFINITY;
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.z = (x3pio_field){row, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    for (size_t r = 0; row != NULL && r < rows && status == X3PIO_OK; ++r)
    {
        status = x3pio_read_points(reader, &into, 1);
        for (size_t c = 0; status == X3PIO_OK && c < columns; ++c)
        {
            if (!isnan(row[c]))
            {
                lowest = fmin(lowest, row[c]);
                highest = fmax(highest, row[c]);
            }
        }
    }
    if (status == X3PIO_OK)
    {
        printf("heights from %g m to %g m\n", lowest, highest);
    }
    else
    {
        fprintf(stderr, "%s\n", x3pio_reader_error_message(reader));
    }
    free(row);
    x3pio_reader_free(reader);
    free(file);
    return status == X3PIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
