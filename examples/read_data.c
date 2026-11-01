#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Reads the numbers of a file of int16_t as they are stored, with the mask of
 * the points that were measured, and converts one row to metres with the
 * increment of the file. The file must store its z coordinates in int16_t. */
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
        fprintf(stderr, "%s\n", x3pio_reader_error_message(reader));
        x3pio_reader_free(reader);
        return EXIT_FAILURE;
    }
    const x3pio_document *document = x3pio_reader_document(reader);
    const x3pio_axis z_axis = x3pio_document_axis(document, X3PIO_Z);
    if (z_axis.storage != X3PIO_TYPE_INT16)
    {
        fprintf(stderr, "the file does not store int16_t\n");
        x3pio_reader_free(reader);
        return EXIT_FAILURE;
    }
    const x3pio_dimensions size = x3pio_document_dimensions(document);
    const size_t count = x3pio_document_point_count(document);
    int16_t *counts = malloc(count * sizeof *counts);
    unsigned char *mask = malloc(count);
    if (counts == NULL || mask == NULL)
    {
        free(counts);
        free(mask);
        x3pio_reader_free(reader);
        return EXIT_FAILURE;
    }
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.z = (x3pio_field){counts, *x3pio_layout_packed(X3PIO_TYPE_INT16)};
    into.valid = (x3pio_field){mask, *x3pio_layout_packed(X3PIO_TYPE_UINT8)};
    status = x3pio_read_data(reader, &into, count);
    if (status == X3PIO_OK)
    {
        size_t measured = 0;
        for (size_t i = 0; i < count; ++i)
        {
            measured += mask[i];
        }
        printf("%zu of %zu points measured, a count is %g m\n", measured, count,
               z_axis.increment);
        /* The conversion needs the document and no reader. */
        double metres[8];
        x3pio_decode_values(document, X3PIO_Z, counts + 5 * size.size_x + 40, 8,
                            x3pio_layout_packed(X3PIO_TYPE_DOUBLE), metres);
        printf("eight z coordinates of v = 5: %g %g ... %g m\n", metres[0],
               metres[1], metres[7]);
    }
    else
    {
        fprintf(stderr, "%s\n", x3pio_reader_error_message(reader));
    }
    free(counts);
    free(mask);
    x3pio_reader_free(reader);
    return status == X3PIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
