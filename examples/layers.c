#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define LAYERS 3
#define COLUMNS 64
#define ROWS 48

/* A file can hold several layers of a surface. They are rows that follow each
 * other, so an image is written or read by as many rows as it has, and the
 * images of an application can be separate arrays. */
int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "layers.x3p";
    static float images[LAYERS][ROWS * COLUMNS];
    for (size_t l = 0; l < LAYERS; ++l)
    {
        for (size_t i = 0; i < (size_t)ROWS * COLUMNS; ++i)
        {
            images[l][i] =
                (float)(1e-6 * (double)(l + 1) * sin(0.01 * (double)i));
        }
    }

    x3pio_document *document = NULL;
    x3pio_document_create(NULL, &document);
    x3pio_document_set_surface(document, COLUMNS, ROWS, 1e-6, 1e-6,
                               X3PIO_TYPE_FLOAT, 1.0, LAYERS);
    x3pio_document_set_software_metadata(document, "layers", "1.0");
    x3pio_writer *writer = NULL;
    x3pio_writer_create(NULL, &writer);
    x3pio_status status = x3pio_create_file(writer, path, document);
    for (size_t l = 0; status == X3PIO_OK && l < LAYERS; ++l)
    {
        x3pio_const_fields from;
        x3pio_const_fields_init(&from);
        from.z = (x3pio_const_field){images[l],
                                     *x3pio_layout_packed(X3PIO_TYPE_FLOAT)};
        status = x3pio_write_points(writer, &from, ROWS);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_end_file(writer);
    }
    x3pio_writer_free(writer);
    x3pio_document_free(document);
    if (status != X3PIO_OK)
    {
        return EXIT_FAILURE;
    }

    x3pio_reader *reader = NULL;
    x3pio_reader_create(NULL, &reader);
    status = x3pio_open_file(reader, path);
    static float back[ROWS * COLUMNS];
    for (size_t l = 0; status == X3PIO_OK && l < LAYERS; ++l)
    {
        x3pio_fields into;
        x3pio_fields_init(&into);
        into.z = (x3pio_field){back, *x3pio_layout_packed(X3PIO_TYPE_FLOAT)};
        status = x3pio_read_points(reader, &into, ROWS);
        if (status == X3PIO_OK)
        {
            printf("layer %zu: first height %g m\n", l, (double)back[1]);
        }
    }
    x3pio_reader_free(reader);
    return status == X3PIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
