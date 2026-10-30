#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define COLUMNS 256
#define ROWS 192

static float heights[ROWS * COLUMNS];

/* Generates a ripple of heights in metres, with a region of points that are not
 * measured, which are NaN. */
static void generate_heights(void)
{
    for (size_t row = 0; row < ROWS; ++row)
    {
        const bool in_region = row > 100 && row < 110;
        const double row_factor = 2e-6 * sin(0.1 * (double)row);
        float *line = heights + row * COLUMNS;
        for (size_t column = 0; column < COLUMNS; ++column)
        {
            line[column] =
                in_region && column < 40
                    ? NAN
                    : (float)(row_factor * cos(0.07 * (double)column));
        }
    }
}

/* Describes the file: a surface of 256 by 192 points 1 micrometre apart, whose
 * heights are stored as `type`. An integer type with X3PIO_AUTO_INCREMENT
 * leaves the increment to the writer, which selects it from the heights. */
static int write_surface(const char *path, x3pio_revision revision,
                         x3pio_type type)
{
    x3pio_document *document = NULL;
    x3pio_document_create(NULL, &document);
    x3pio_document_set_revision(document, revision);
    x3pio_document_set_surface(
        document, COLUMNS, ROWS, 1e-6, 1e-6, type,
        type == X3PIO_TYPE_FLOAT ? 1.0 : X3PIO_AUTO_INCREMENT, 1);

    /* The metadata is optional. A file that has it needs a date and the kind of
     * the probing system. This is the shortcut for data that software made. */
    x3pio_document_set_software_metadata(document, "write_heights", "1.0");
    x3pio_document_set_creator(document, "Jane Doe");

    x3pio_writer *writer = NULL;
    x3pio_writer_create(NULL, &writer);
    x3pio_status status = x3pio_create_file(writer, path, document);
    if (status == X3PIO_OK)
    {
        /* The layout says how the heights lie in memory: here, floats that
         * follow each other. */
        status = x3pio_write_heights(
            writer, x3pio_layout_packed(X3PIO_TYPE_FLOAT), heights);
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

int main(int argc, char **argv)
{
    const char *prefix = argc > 1 ? argv[1] : "surface";
    generate_heights();
    char path[512];
    int failed = 0;
    snprintf(path, sizeof path, "%s_float.x3p", prefix);
    failed |= write_surface(path, X3PIO_REVISION_AMD1, X3PIO_TYPE_FLOAT);
    snprintf(path, sizeof path, "%s_int16.x3p", prefix);
    failed |= write_surface(path, X3PIO_REVISION_AMD1, X3PIO_TYPE_INT16);
    snprintf(path, sizeof path, "%s_2017.x3p", prefix);
    failed |= write_surface(path, X3PIO_REVISION_2017, X3PIO_TYPE_INT32);
    return failed;
}
