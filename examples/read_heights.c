#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s file.x3p\n", argv[0]);
        return EXIT_FAILURE;
    }
    x3pio_reader *reader = NULL;
    x3pio_reader_create(NULL, &reader);
    x3pio_status status = x3pio_open_file(reader, argv[1]);
    if (status != X3PIO_OK)
    {
        fprintf(stderr, "%s: %s\n", argv[1],
                x3pio_reader_error_message(reader));
        x3pio_reader_free(reader);
        return EXIT_FAILURE;
    }

    /* The description is read when the file is opened, before any point. The
     * program learns the size from it and allocates the buffer itself. */
    const x3pio_document *document = x3pio_reader_document(reader);
    const size_t columns = x3pio_document_columns(document);
    const size_t rows = x3pio_document_rows(document);
    printf("%zu columns, %zu rows, x step %g m, y step %g m\n", columns, rows,
           x3pio_document_axis(document, X3PIO_X).increment,
           x3pio_document_axis(document, X3PIO_Y).increment);
    printf("created by %s\n", x3pio_document_creator(document));

    double *heights = malloc(columns * rows * sizeof *heights);
    if (heights == NULL)
    {
        x3pio_reader_free(reader);
        return EXIT_FAILURE;
    }
    status = x3pio_read_heights(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE),
                                heights);
    if (status != X3PIO_OK)
    {
        fprintf(stderr, "%s\n", x3pio_reader_error_message(reader));
    }
    else
    {
        /* A point that was not measured is NaN. */
        size_t measured = 0;
        double sum = 0.0;
        for (size_t i = 0; i < columns * rows; ++i)
        {
            if (!isnan(heights[i]))
            {
                ++measured;
                sum += heights[i];
            }
        }
        printf("%zu points measured, mean height %g m\n", measured,
               measured > 0 ? sum / (double)measured : 0.0);
    }
    free(heights);
    x3pio_reader_free(reader);
    return status == X3PIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
