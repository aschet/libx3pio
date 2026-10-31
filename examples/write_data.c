#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define COLUMNS 200
#define ROWS 100

/* The image of a sensor in counts of int16_t, 1000 counts being 2.5
 * micrometres. It is written as it is, without a number in floating point in
 * the memory of the program. Integers have no value for a point that was not
 * measured, so the program passes a mask: a byte for each point, 1 for a point
 * that was measured and 0 for one that was not. */
static int16_t counts[ROWS * COLUMNS];
static unsigned char mask[ROWS * COLUMNS];

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "counts.x3p";
    for (size_t row = 0; row < ROWS; ++row)
    {
        for (size_t column = 0; column < COLUMNS; ++column)
        {
            const size_t at = row * COLUMNS + column;
            counts[at] = (int16_t)((int)row * 7 - (int)column * 3);
            /* The sensor delivers no value in a corner. */
            mask[at] = (unsigned char)(row < 30 && column < 30 ? 0 : 1);
        }
    }

    x3pio_document *document = NULL;
    x3pio_document_create(&document);
    /* One count is 2.5 nanometres. */
    x3pio_document_set_surface(document, COLUMNS, ROWS, 1e-6, 1e-6,
                               X3PIO_TYPE_INT16, 2.5e-9, 1);
    x3pio_document_set_software_metadata(document, "write_data", "1.0");

    x3pio_writer *writer = NULL;
    x3pio_writer_create(&writer);
    x3pio_status status = x3pio_create_file(writer, path, document);
    if (status == X3PIO_OK)
    {
        /* The type of a layout is the type in which the file stores the axis.
         */
        x3pio_const_fields from;
        x3pio_const_fields_init(&from);
        from.z =
            (x3pio_const_field){counts, *x3pio_layout_packed(X3PIO_TYPE_INT16)};
        from.valid =
            (x3pio_const_field){mask, *x3pio_layout_packed(X3PIO_TYPE_UINT8)};
        status = x3pio_write_data(writer, &from, ROWS);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_end_file(writer);
    }
    if (status != X3PIO_OK)
    {
        fprintf(stderr, "%s\n", x3pio_writer_error_message(writer));
    }
    x3pio_writer_free(writer);
    x3pio_document_free(document);
    return status == X3PIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
