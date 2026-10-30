#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A stream that appends to a buffer of the program that grows, so that a file
 * can be made in memory, for example to send it over a network. */
typedef struct membuf
{
    unsigned char *data;
    size_t size;
    size_t capacity;
} membuf;

static size_t membuf_write(void *user, const void *data, size_t size)
{
    membuf *buffer = user;
    if (buffer->size + size > buffer->capacity)
    {
        size_t capacity = buffer->capacity != 0 ? buffer->capacity : 4096;
        while (capacity < buffer->size + size)
        {
            capacity *= 2;
        }
        unsigned char *grown = realloc(buffer->data, capacity);
        if (grown == NULL)
        {
            return 0;
        }
        buffer->data = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
    return size;
}

int main(void)
{
    membuf buffer = {NULL, 0, 0};
    x3pio_stream stream;
    x3pio_stream_init(&stream);
    stream.user = &buffer;
    stream.write = membuf_write;

    x3pio_document *document = NULL;
    x3pio_document_create(NULL, &document);
    x3pio_document_set_surface(document, 4, 3, 1e-6, 1e-6, X3PIO_TYPE_FLOAT,
                               1.0, 1);
    x3pio_document_set_software_metadata(document, "write_memory", "1.0");
    const double heights[12] = {1e-7, 2e-7, 3e-7, 4e-7, 5e-7,   6e-7,
                                7e-7, 8e-7, 9e-7, 1e-6, 1.1e-6, 1.2e-6};
    x3pio_writer *writer = NULL;
    x3pio_writer_create(NULL, &writer);
    x3pio_status status = x3pio_create_stream(writer, &stream, document);
    if (status == X3PIO_OK)
    {
        status = x3pio_write_heights(
            writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), heights);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_end_file(writer);
    }
    x3pio_writer_free(writer);
    x3pio_document_free(document);
    if (status != X3PIO_OK)
    {
        free(buffer.data);
        return EXIT_FAILURE;
    }
    printf("the file has %zu bytes\n", buffer.size);

    /* And back from memory, where the reader reads the bytes where they are. */
    x3pio_reader *reader = NULL;
    x3pio_reader_create(NULL, &reader);
    status = x3pio_open_memory(reader, buffer.data, buffer.size);
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
    free(buffer.data);
    return status == X3PIO_OK ? EXIT_SUCCESS : EXIT_FAILURE;
}
