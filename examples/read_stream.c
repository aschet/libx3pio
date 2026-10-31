#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/* Reads a file from standard input, for example from a pipe:
 *
 *     cat surface.x3p | read_stream
 *
 * A zip file lists its members at its end, so the reader collects a stream in
 * memory before it opens the file. Then it reads the rows in pieces, here a row
 * at a time, so the program needs the memory of one row of metres. */
int main(void)
{
    x3pio_reader *reader = NULL;
    x3pio_reader_create(&reader);
    x3pio_stream stream;
    x3pio_stdio_stream(&stream, stdin);
    x3pio_status status = x3pio_open_stream(reader, &stream);
    if (status != X3PIO_OK)
    {
        fprintf(stderr, "%s\n", x3pio_reader_error_message(reader));
        x3pio_reader_free(reader);
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
    return status == X3PIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
