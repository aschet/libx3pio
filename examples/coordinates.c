#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct point
{
    float x;
    float y;
    float z;
} point;

enum
{
    COUNT = 4
};

/* A part that sits in the coordinate system of a machine: turned by 90 degrees
 * about z and moved by (250, -120, 5) mm. The file stores the points in the
 * view coordinate system of the part, and the coordinate transformation of the
 * standard, `global = R * (I * stored) + O`, says where the part lies in the
 * global coordinate system of the machine. */
static x3pio_transformation machine_transformation(void)
{
    x3pio_transformation transformation = x3pio_transformation_identity();
    transformation.rotation[0] = 0.0;
    transformation.rotation[1] = -1.0;
    transformation.rotation[3] = 1.0;
    transformation.rotation[4] = 0.0;
    transformation.offset[0] = 0.250;
    transformation.offset[1] = -0.120;
    transformation.offset[2] = 0.005;
    return transformation;
}

/* The program has its points in the coordinate system of the machine, as a
 * scanner or a CAD program gives them. The writer takes them from there:
 * it undoes the transformation for each point, in double precision, and stores
 * the points of the part. A float is enough for the program, but not for
 * this arithmetic: a coordinate of 0.25 m with a feature of 1 micrometre is at
 * the limit of what a float resolves. */
static x3pio_status write_part(const char *path)
{
    const point global[COUNT] = {{0.2500F, -0.1200F, 0.005000F},
                                 {0.2499F, -0.1199F, 0.005001F},
                                 {0.2498F, -0.1198F, 0.005002F},
                                 {0.2497F, -0.1197F, 0.005003F}};
    const x3pio_transformation transformation = machine_transformation();
    x3pio_document *document = NULL;
    x3pio_document_create(&document);
    x3pio_document_describe_point_cloud(document, COUNT, X3PIO_TYPE_DOUBLE,
                                        1.0);
    x3pio_document_set_transformation(document, &transformation);
    x3pio_writer *writer = NULL;
    x3pio_writer_create(&writer);
    x3pio_status status = x3pio_create_file(writer, path, document);
    if (status == X3PIO_OK)
    {
        x3pio_const_fields from;
        x3pio_const_fields_init(&from);
        const ptrdiff_t stride = sizeof(point);
        from.x =
            (x3pio_const_field){&global[0].x, {X3PIO_TYPE_FLOAT, stride, 0}};
        from.y =
            (x3pio_const_field){&global[0].y, {X3PIO_TYPE_FLOAT, stride, 0}};
        from.z =
            (x3pio_const_field){&global[0].z, {X3PIO_TYPE_FLOAT, stride, 0}};
        status = x3pio_write_points_in(writer, X3PIO_COORDINATES_GLOBAL, &from,
                                       COUNT);
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
    return status;
}

/* Reads the last point of the file in one coordinate system. */
static x3pio_status read_last(const char *path, x3pio_coordinate_system system,
                              const char *name)
{
    point points[COUNT];
    x3pio_reader *reader = NULL;
    x3pio_reader_create(&reader);
    x3pio_status status = x3pio_open_file(reader, path);
    if (status == X3PIO_OK)
    {
        x3pio_fields into;
        x3pio_fields_init(&into);
        const ptrdiff_t stride = sizeof(point);
        into.x = (x3pio_field){&points[0].x, {X3PIO_TYPE_FLOAT, stride, 0}};
        into.y = (x3pio_field){&points[0].y, {X3PIO_TYPE_FLOAT, stride, 0}};
        into.z = (x3pio_field){&points[0].z, {X3PIO_TYPE_FLOAT, stride, 0}};
        status = x3pio_read_points_in(reader, system, &into, COUNT);
    }
    if (status == X3PIO_OK)
    {
        printf("%-16s (%.6f, %.6f, %.6f) m\n", name,
               (double)points[COUNT - 1].x, (double)points[COUNT - 1].y,
               (double)points[COUNT - 1].z);
    }
    else
    {
        fprintf(stderr, "%s\n", x3pio_reader_error_message(reader));
    }
    x3pio_reader_free(reader);
    return status;
}

/* The same file in the three coordinate systems that a program can ask for:
 * - the view coordinate system, in which the file stores the part:
 * x3pio_read_points() is the same;
 * - the view coordinates plus the offset, without the rotation, which is what
 *   software gave that ignores the rotation of the file;
 * - the global coordinate system of the machine. */
int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "coordinates.x3p";
    x3pio_status status = write_part(path);
    if (status == X3PIO_OK)
    {
        status = read_last(path, X3PIO_COORDINATES_VIEW, "view:");
    }
    if (status == X3PIO_OK)
    {
        status =
            read_last(path, X3PIO_COORDINATES_VIEW_OFFSET, "view + offset:");
    }
    if (status == X3PIO_OK)
    {
        status = read_last(path, X3PIO_COORDINATES_GLOBAL, "global:");
    }
    return status == X3PIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
