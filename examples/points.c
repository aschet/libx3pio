#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

/* The points of the application, with other members around the coordinates. */
typedef struct point
{
    float x;
    float y;
    float z;
    unsigned char intensity;
} point;

/* Writes a cloud of 1000 points that lie on a spiral. */
static int write_cloud(const char *path)
{
    enum
    {
        COUNT = 1000
    };
    point points[COUNT];
    for (size_t i = 0; i < COUNT; ++i)
    {
        const double angle = 0.05 * (double)i;
        points[i] =
            (point){(float)(1e-3 * cos(angle) * (1.0 + 0.002 * (double)i)),
                    (float)(1e-3 * sin(angle) * (1.0 + 0.002 * (double)i)),
                    (float)(1e-6 * (double)i), (unsigned char)(i & 0xFF)};
    }
    x3pio_document *document = NULL;
    x3pio_document_create(&document);
    x3pio_document_set_feature_point_cloud(document, COUNT, X3PIO_TYPE_FLOAT,
                                           1.0);
    x3pio_document_set_software_metadata(document, "points", "1.0");
    x3pio_writer *writer = NULL;
    x3pio_writer_create(&writer);
    x3pio_status status = x3pio_create_file(writer, path, document);
    if (status == X3PIO_OK)
    {
        /* The coordinates are members of the structure: the element stride is
         * the size of the structure. A list of points is written in any number
         * of parts. */
        x3pio_const_fields from;
        x3pio_const_fields_init(&from);
        const ptrdiff_t stride = sizeof(point);
        from.x =
            (x3pio_const_field){&points[0].x, {X3PIO_TYPE_FLOAT, stride, 0}};
        from.y =
            (x3pio_const_field){&points[0].y, {X3PIO_TYPE_FLOAT, stride, 0}};
        from.z =
            (x3pio_const_field){&points[0].z, {X3PIO_TYPE_FLOAT, stride, 0}};
        status = x3pio_write_points(writer, &from, COUNT);
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

/* Reads the points of any file, whether it holds a cloud, a profile or a
 * surface: the program does not need to know. A grid has no stored x and y, and
 * the reader gives them from the place of the point. The points come in pieces
 * that fit an array of the program, so a cloud of any size needs the memory of
 * one piece. A list takes any number of points in a piece; a matrix is read in
 * whole runs along u of size_x points, which must fit the array. */
static int read_points(const char *path)
{
    enum
    {
        PIECE = 256
    };
    point piece[PIECE];
    x3pio_reader *reader = NULL;
    x3pio_reader_create(&reader);
    x3pio_status status = x3pio_open_file(reader, path);
    size_t count = 0;
    size_t measured = 0;
    point last = {0, 0, 0, 0};
    if (status == X3PIO_OK)
    {
        const x3pio_document *document = x3pio_reader_document(reader);
        const x3pio_dimensions size = x3pio_document_dimensions(document);
        const size_t total = x3pio_document_point_count(document);
        /* A matrix is read in whole runs along u of size_x points, a list in
         * any number of points. */
        const size_t run = size.list ? 1 : size.size_x;
        const size_t points_per_piece = PIECE / run * run;
        if (points_per_piece == 0)
        {
            status = X3PIO_ERR_ARGUMENT;
        }
        x3pio_fields into;
        x3pio_fields_init(&into);
        const ptrdiff_t stride = sizeof(point);
        into.x = (x3pio_field){&piece[0].x, {X3PIO_TYPE_FLOAT, stride, 0}};
        into.y = (x3pio_field){&piece[0].y, {X3PIO_TYPE_FLOAT, stride, 0}};
        into.z = (x3pio_field){&piece[0].z, {X3PIO_TYPE_FLOAT, stride, 0}};
        for (size_t first = 0; status == X3PIO_OK && first < total;
             first += points_per_piece)
        {
            const size_t n = total - first < points_per_piece
                                 ? total - first
                                 : points_per_piece;
            status = x3pio_read_points(reader, &into, n);
            for (size_t i = 0; status == X3PIO_OK && i < n; ++i)
            {
                measured += isnan(piece[i].z) ? 0U : 1U;
                last = piece[i];
            }
            count += n;
        }
    }
    if (status == X3PIO_OK)
    {
        printf("%zu points, %zu measured; the last one is (%g, %g, %g) m\n",
               count, measured, (double)last.x, (double)last.y, (double)last.z);
    }
    else
    {
        fprintf(stderr, "%s\n", x3pio_reader_error_message(reader));
    }
    x3pio_reader_free(reader);
    return status == X3PIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "cloud.x3p";
    return write_cloud(path) | read_points(path);
}
