#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A vendor puts files of its own in the container, for example the settings of
 * the instrument. A file is added with the vendor and its path below it, and
 * has the identifier that is both, whatever the revision of the standard: in
 * Amd 1 the identifier also names the file in the container, and in
 * ISO 25178-72:2017 the vendor is the one identifier of the extension and the
 * path is the name of the file. A reader reads only the file that it is asked
 * for, by its identifier. */
int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "vendor.x3p";
    static const char settings[] = "exposure=12ms\nobjective=50x\n";
    x3pio_document *document = NULL;
    x3pio_document_create(&document);
    x3pio_document_set_feature_surface(document, 4, 3, 1e-6, 1e-6,
                                       X3PIO_TYPE_FLOAT, 1.0, 1);
    x3pio_document_set_software_metadata(document, "vendor_files", "1.0");
    const double heights[12] = {0};

    x3pio_writer *writer = NULL;
    x3pio_writer_create(&writer);
    x3pio_status status = x3pio_create_file(writer, path, document);
    if (status == X3PIO_OK)
    {
        status = x3pio_add_vendor_file(writer, "http://www.example.com",
                                       "settings.ini", settings,
                                       strlen(settings), NULL);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_write_z(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE),
                               heights);
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
    x3pio_reader_create(&reader);
    status = x3pio_open_file(reader, path);
    for (size_t i = 0;
         status == X3PIO_OK && i < x3pio_reader_vendor_file_count(reader); ++i)
    {
        const char *id = x3pio_reader_vendor_file_id(reader, i);
        const char *member = x3pio_reader_vendor_file_member(reader, i);
        printf("%s is the file %s\n", id, member != NULL ? member : "(none)");
        if (member == NULL)
        {
            continue;
        }
        /* The size first, then the bytes. */
        size_t size = 0;
        (void)x3pio_read_vendor_file(reader, id, NULL, 0, &size);
        char *content = malloc(size + 1);
        if (content != NULL
            && x3pio_read_vendor_file(reader, id, content, size, &size)
                   == X3PIO_OK)
        {
            content[size] = '\0';
            printf("%s", content);
        }
        free(content);
    }
    x3pio_reader_free(reader);
    return status == X3PIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
