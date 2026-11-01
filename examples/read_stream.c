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
 * first, and opens that. The points are then read in pieces, here a run along
 * u at a time, so the program needs the memory of the zip file and of one run
 * of z coordinates in metres. */
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
    const x3pio_dimensions extent = x3pio_document_dimensions(document);
    const size_t count = x3pio_document_point_count(document);
    /* A matrix is read in whole runs along u of size_x points, a list in any
     * number of points. */
    const size_t piece = extent.list ? 256 : extent.size_x;
    double *z = malloc(piece * sizeof *z);
    double lowest = INFINITY;
    double highest = -INFINITY;
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.z = (x3pio_field){z, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    for (size_t first = 0; z != NULL && first < count && status == X3PIO_OK;
         first += piece)
    {
        const size_t n = count - first < piece ? count - first : piece;
        status = x3pio_read_points(reader, &into, n);
        for (size_t i = 0; status == X3PIO_OK && i < n; ++i)
        {
            if (!isnan(z[i]))
            {
                lowest = fmin(lowest, z[i]);
                highest = fmax(highest, z[i]);
            }
        }
    }
    if (status == X3PIO_OK)
    {
        printf("z from %g m to %g m\n", lowest, highest);
    }
    else
    {
        fprintf(stderr, "%s\n", x3pio_reader_error_message(reader));
    }
    free(z);
    x3pio_reader_free(reader);
    free(file);
    return status == X3PIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
