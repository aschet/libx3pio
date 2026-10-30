#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* An allocator that counts what the library asks for. A program can put a pool
 * or a bump allocator in its place. The handles ask for memory when they are
 * created and when a file is opened or created, and not while rows go through.
 * The libraries for the zip file, minizip-ng and zlib in it, use malloc and are
 * not reached. */
typedef struct counting
{
    size_t blocks;
    size_t bytes;
    size_t live;
} counting;

typedef struct header
{
    size_t size;
    max_align_t alignment;
} header;

static void *counting_allocate(void *user, size_t size)
{
    counting *counts = user;
    header *block = malloc(sizeof *block + size);
    if (block == NULL)
    {
        return NULL;
    }
    block->size = size;
    ++counts->blocks;
    ++counts->live;
    counts->bytes += size;
    return block + 1;
}

static void counting_release(void *user, void *memory)
{
    counting *counts = user;
    --counts->live;
    free((header *)memory - 1);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s file.x3p\n", argv[0]);
        return EXIT_FAILURE;
    }
    counting counts = {0, 0, 0};
    const x3pio_allocator allocator = {&counts, counting_allocate,
                                       counting_release};
    x3pio_reader *reader = NULL;
    x3pio_status status = x3pio_reader_create(&allocator, &reader);
    if (status == X3PIO_OK)
    {
        status = x3pio_open_file(reader, argv[1]);
    }
    if (status == X3PIO_OK)
    {
        const x3pio_document *document = x3pio_reader_document(reader);
        printf("%zu x %zu points; %zu blocks, %zu bytes so far\n",
               x3pio_document_columns(document), x3pio_document_rows(document),
               counts.blocks, counts.bytes);
    }
    x3pio_reader_free(reader);
    printf("%zu blocks are still in use after the reader is freed\n",
           counts.live);
    return status == X3PIO_OK && counts.live == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
