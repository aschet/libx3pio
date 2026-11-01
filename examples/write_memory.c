#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <stdio.h>
#include <stdlib.h>

/* Makes a file in memory, for example to send it over a network, and reads it
 * back from memory. The writer holds the bytes until it is freed. */
int main(void)
{
    x3pio_document *document = NULL;
    x3pio_document_create(&document);
    x3pio_document_describe_surface(document, 4, 3, 1e-6, 1e-6,
                                    X3PIO_TYPE_FLOAT, 1.0, 1);
    x3pio_document_set_software_metadata(document, "write_memory", "1.0");
    const double heights[12] = {1e-7, 2e-7, 3e-7, 4e-7, 5e-7,   6e-7,
                                7e-7, 8e-7, 9e-7, 1e-6, 1.1e-6, 1.2e-6};
    x3pio_writer *writer = NULL;
    x3pio_writer_create(&writer);
    x3pio_status status = x3pio_create_memory(writer, document);
    if (status == X3PIO_OK)
    {
        status = x3pio_write_heights(
            writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), heights);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_end_file(writer);
    }
    const void *file = NULL;
    size_t size = 0;
    if (status == X3PIO_OK)
    {
        status = x3pio_writer_memory(writer, &file, &size);
    }
    if (status != X3PIO_OK)
    {
        fprintf(stderr, "%s\n", x3pio_writer_error_message(writer));
        x3pio_writer_free(writer);
        x3pio_document_free(document);
        return EXIT_FAILURE;
    }
    printf("the file has %zu bytes\n", size);

    /* The bytes are the program's to send, store or open. */
    x3pio_reader *reader = NULL;
    x3pio_reader_create(&reader);
    status = x3pio_open_memory(reader, file, size);
    double back[12];
    if (status == X3PIO_OK)
    {
        status = x3pio_read_heights(
            reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), back);
    }
    if (status == X3PIO_OK)
    {
        printf("the last height is %g m\n", back[11]);
    }
    x3pio_reader_free(reader);
    x3pio_writer_free(writer);
    x3pio_document_free(document);
    return status == X3PIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
