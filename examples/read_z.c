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
    x3pio_reader_create(&reader);
    x3pio_status status = x3pio_open_file(reader, argv[1]);
    if (status != X3PIO_OK)
    {
        fprintf(stderr, "%s: %s\n", argv[1],
                x3pio_reader_error_message(reader));
        x3pio_reader_free(reader);
        return EXIT_FAILURE;
    }

    /* The document is read when the file is opened, before any point. The
     * program learns the size from it and allocates the buffer itself. */
    const x3pio_document *document = x3pio_reader_document(reader);
    const x3pio_dimensions size = x3pio_document_dimensions(document);
    const size_t count = x3pio_document_point_count(document);
    printf("%zu by %zu points in %zu layers, x step %g m, y step %g m\n",
           size.size_x, size.size_y, size.size_z,
           x3pio_document_axis(document, X3PIO_X).increment,
           x3pio_document_axis(document, X3PIO_Y).increment);
    printf("created by %s\n", x3pio_document_creator(document));

    double *z = malloc(count * sizeof *z);
    if (z == NULL)
    {
        x3pio_reader_free(reader);
        return EXIT_FAILURE;
    }
    status = x3pio_read_z(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z);
    if (status != X3PIO_OK)
    {
        fprintf(stderr, "%s\n", x3pio_reader_error_message(reader));
    }
    else
    {
        /* A point that was not measured is NaN. */
        size_t measured = 0;
        double sum = 0.0;
        for (size_t i = 0; i < count; ++i)
        {
            if (!isnan(z[i]))
            {
                ++measured;
                sum += z[i];
            }
        }
        printf("%zu points measured, mean z %g m\n", measured,
               measured > 0 ? sum / (double)measured : 0.0);
    }
    free(z);
    x3pio_reader_free(reader);
    return status == X3PIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
