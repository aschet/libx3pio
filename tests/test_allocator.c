/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The allocator of the program: every block is released, a handle allocates
 * when it is created and when a file is opened or created and not while rows
 * are transferred, and a failed allocation is reported and leaves nothing
 * behind. */

#include "helpers.h"

#include <x3pio/file.h>

#include <math.h>
#include <stdint.h>
#include <string.h>

#define MAX_BLOCKS 200000

typedef struct counter
{
    void *blocks[MAX_BLOCKS];
    size_t sizes[MAX_BLOCKS];
    size_t live;
    /* The bytes that are allocated now, and the most there were. */
    size_t live_bytes;
    size_t peak_bytes;
    size_t calls;
    /* The number of the allocation that fails, counted from 1; 0 for none. */
    size_t fail_at;
    size_t foreign_releases;
    size_t misaligned;
} counter;

static void *counting_allocate(void *user, size_t size)
{
    counter *c = user;
    ++c->calls;
    if (c->fail_at != 0 && c->calls == c->fail_at)
    {
        return NULL;
    }
    void *block = malloc(size);
    if (block == NULL || c->live >= MAX_BLOCKS)
    {
        free(block);
        return NULL;
    }
    if ((uintptr_t)block % 16 != 0)
    {
        ++c->misaligned;
    }
    c->sizes[c->live] = size;
    c->blocks[c->live++] = block;
    c->live_bytes += size;
    if (c->live_bytes > c->peak_bytes)
    {
        c->peak_bytes = c->live_bytes;
    }
    return block;
}

static void counting_release(void *user, void *memory)
{
    counter *c = user;
    for (size_t i = 0; i < c->live; ++i)
    {
        if (c->blocks[i] == memory)
        {
            c->live_bytes -= c->sizes[i];
            --c->live;
            c->blocks[i] = c->blocks[c->live];
            c->sizes[i] = c->sizes[c->live];
            free(memory);
            return;
        }
    }
    ++c->foreign_releases;
}

static x3pio_allocator allocator_of(counter *c)
{
    return (x3pio_allocator){c, counting_allocate, counting_release};
}

static void reset(counter *c, size_t fail_at)
{
    c->live = 0;
    c->live_bytes = 0;
    c->peak_bytes = 0;
    c->calls = 0;
    c->fail_at = fail_at;
    c->foreign_releases = 0;
    c->misaligned = 0;
}

static x3pio_buffer sample(x3pio_storage storage, bool vendor)
{
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(NULL, &doc));
    CHECK_OK(x3pio_document_set_surface(doc, 4, 3, 1e-6, 1e-6, X3PIO_TYPE_INT16,
                                        1e-9, 1));
    x3pio_timestamp date = {2026, 3, 4, 12, 30, 0, 0, true, 60};
    CHECK_OK(x3pio_document_set_date(doc, &date));
    CHECK_OK(x3pio_document_set_probing_type(doc, X3PIO_PROBING_SOFTWARE));
    CHECK_OK(x3pio_document_set_comment(doc, "an allocator test"));
    int16_t counts[12];
    unsigned char mask[12];
    for (size_t i = 0; i < 12; ++i)
    {
        counts[i] = (int16_t)(100 * (int)i);
        mask[i] = i != 5;
    }
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(NULL, &writer));
    CHECK_OK(x3pio_writer_set_storage(writer, storage));
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    x3pio_const_fields from;
    x3pio_const_fields_init(&from);
    from.z =
        (x3pio_const_field){counts, *x3pio_layout_packed(X3PIO_TYPE_INT16)};
    from.valid =
        (x3pio_const_field){mask, *x3pio_layout_packed(X3PIO_TYPE_UINT8)};
    CHECK_OK(x3pio_write_data(writer, &from, 3));
    if (vendor)
    {
        CHECK_OK(x3pio_add_vendor_file(writer, "http://v.org/x", NULL, "data",
                                       4, NULL));
    }
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);
    x3pio_document_free(doc);
    return bytes;
}

/* One reading with an allocator of the program. The number of allocations that
 * the transfer of rows made is returned in `during`. */
static x3pio_status read_cycle(counter *c, const x3pio_buffer *bytes,
                               size_t *during)
{
    const x3pio_allocator allocator = allocator_of(c);
    x3pio_reader *reader = NULL;
    x3pio_status status = x3pio_reader_create(&allocator, &reader);
    if (status != X3PIO_OK)
    {
        return status;
    }
    status = x3pio_open_memory(reader, bytes->data, bytes->size);
    if (status == X3PIO_OK)
    {
        const size_t before = c->calls;
        int16_t counts[12];
        unsigned char mask[12];
        x3pio_fields into;
        x3pio_fields_init(&into);
        into.z = (x3pio_field){counts, *x3pio_layout_packed(X3PIO_TYPE_INT16)};
        into.valid =
            (x3pio_field){mask, *x3pio_layout_packed(X3PIO_TYPE_UINT8)};
        status = x3pio_read_data(reader, &into, 1);
        if (status == X3PIO_OK)
        {
            status = x3pio_read_data(reader, &into, 0);
        }
        float metres[12];
        x3pio_fields in_metres;
        x3pio_fields_init(&in_metres);
        in_metres.z =
            (x3pio_field){metres, *x3pio_layout_packed(X3PIO_TYPE_FLOAT)};
        if (status == X3PIO_OK)
        {
            status = x3pio_read_points(reader, &in_metres, 2);
        }
        *during = c->calls - before;
        if (status == X3PIO_OK && x3pio_reader_vendor_file_count(reader) > 0)
        {
            unsigned char content[8];
            size_t size = 0;
            status = x3pio_read_member(reader, "v/org/x", content,
                                       sizeof content, &size);
        }
    }
    x3pio_reader_free(reader);
    return status;
}

static x3pio_status write_cycle(counter *c, x3pio_storage storage, bool vendor,
                                bool to_file, size_t *during)
{
    const x3pio_allocator allocator = allocator_of(c);
    x3pio_document *doc = NULL;
    x3pio_status status = x3pio_document_create(&allocator, &doc);
    x3pio_writer *writer = NULL;
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    if (status == X3PIO_OK)
    {
        status = x3pio_document_set_surface(doc, 4, 3, 1e-6, 1e-6,
                                            X3PIO_TYPE_INT16, 1e-9, 1);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_document_set_comment(doc, "an allocator test");
    }
    if (status == X3PIO_OK)
    {
        x3pio_timestamp date = {2026, 3, 4, 12, 30, 0, 0, true, 60};
        status = x3pio_document_set_date(doc, &date);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_document_set_probing_type(doc, X3PIO_PROBING_SOFTWARE);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_writer_create(&allocator, &writer);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_writer_set_storage(writer, storage);
    }
    if (status == X3PIO_OK)
    {
        status = to_file ? x3pio_create_file(writer, "test_allocator.x3p", doc)
                         : create_in_buffer(writer, doc, &bytes);
    }
    if (status == X3PIO_OK)
    {
        const size_t before = c->calls;
        int16_t counts[12];
        unsigned char mask[12];
        for (size_t i = 0; i < 12; ++i)
        {
            counts[i] = (int16_t)(100 * (int)i);
            mask[i] = i != 5;
        }
        x3pio_const_fields from;
        x3pio_const_fields_init(&from);
        from.z =
            (x3pio_const_field){counts, *x3pio_layout_packed(X3PIO_TYPE_INT16)};
        from.valid =
            (x3pio_const_field){mask, *x3pio_layout_packed(X3PIO_TYPE_UINT8)};
        status = x3pio_write_data(writer, &from, 2);
        if (status == X3PIO_OK)
        {
            status = x3pio_write_data(writer, &from, 1);
        }
        *during = c->calls - before;
    }
    if (status == X3PIO_OK && vendor)
    {
        status = x3pio_add_vendor_file(writer, "http://v.org/x", NULL, "data",
                                       4, NULL);
    }
    if (status == X3PIO_OK)
    {
        status = x3pio_end_file(writer);
    }
    x3pio_writer_free(writer);
    x3pio_document_free(doc);
    x3pio_buffer_free(&bytes);
    return status;
}

static void test_balanced(void)
{
    static counter c;
    for (int st = 0; st < 2; ++st)
    {
        const x3pio_storage storage =
            st == 0 ? X3PIO_STORAGE_BINARY : X3PIO_STORAGE_TEXT;
        x3pio_buffer bytes = sample(storage, true);
        reset(&c, 0);
        size_t during = 99;
        CHECK_OK(read_cycle(&c, &bytes, &during));
        CHECK(c.live == 0 && c.foreign_releases == 0 && c.misaligned == 0);
        CHECK(c.calls > 5);
        /* The transfer of rows allocates nothing. */
        CHECK(during == 0);

        /* A file on disk is written without allocations while rows go out. A
         * file on a stream is collected in memory, which grows. */
        reset(&c, 0);
        during = 99;
        CHECK_OK(write_cycle(&c, storage, true, true, &during));
        CHECK(c.live == 0 && c.foreign_releases == 0 && c.misaligned == 0);
        CHECK(c.calls > 5);
        CHECK(during == 0);
        remove("test_allocator.x3p");
        reset(&c, 0);
        CHECK_OK(write_cycle(&c, storage, true, false, &during));
        CHECK(c.live == 0 && c.foreign_releases == 0 && c.misaligned == 0);
        x3pio_buffer_free(&bytes);
    }
}

static void test_failures(void)
{
    static counter c;
    for (int st = 0; st < 2; ++st)
    {
        const x3pio_storage storage =
            st == 0 ? X3PIO_STORAGE_BINARY : X3PIO_STORAGE_TEXT;
        x3pio_buffer bytes = sample(storage, true);
        reset(&c, 0);
        size_t during = 0;
        CHECK_OK(read_cycle(&c, &bytes, &during));
        const size_t read_calls = c.calls;
        for (size_t fail = 1; fail <= read_calls + 1; ++fail)
        {
            reset(&c, fail);
            const x3pio_status status = read_cycle(&c, &bytes, &during);
            CHECK(status == X3PIO_OK || status == X3PIO_ERR_MEMORY);
            if (fail <= read_calls)
            {
                CHECK(status == X3PIO_ERR_MEMORY);
            }
            CHECK(c.live == 0 && c.foreign_releases == 0);
            if (c.live != 0
                || (fail <= read_calls && status != X3PIO_ERR_MEMORY))
            {
                fprintf(stderr, "  ^ read, allocation %zu failed (%s): %s\n",
                        fail, st == 0 ? "binary" : "text",
                        x3pio_status_string(status));
            }
        }
        reset(&c, 0);
        CHECK_OK(write_cycle(&c, storage, true, false, &during));
        const size_t write_calls = c.calls;
        for (size_t fail = 1; fail <= write_calls + 1; ++fail)
        {
            reset(&c, fail);
            const x3pio_status status =
                write_cycle(&c, storage, true, false, &during);
            CHECK(status == X3PIO_OK || status == X3PIO_ERR_MEMORY);
            if (fail <= write_calls)
            {
                CHECK(status == X3PIO_ERR_MEMORY);
            }
            CHECK(c.live == 0 && c.foreign_releases == 0);
            if (c.live != 0
                || (fail <= write_calls && status != X3PIO_ERR_MEMORY))
            {
                fprintf(stderr, "  ^ write, allocation %zu failed (%s): %s\n",
                        fail, st == 0 ? "binary" : "text",
                        x3pio_status_string(status));
            }
        }
        x3pio_buffer_free(&bytes);
    }
}

static void test_arguments(void)
{
    /* Both callbacks are required, and null selects malloc and free. */
    counter c;
    reset(&c, 0);
    x3pio_allocator allocator = allocator_of(&c);
    x3pio_reader *reader = NULL;
    x3pio_writer *writer = NULL;
    x3pio_document *doc = NULL;
    allocator.release = NULL;
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_reader_create(&allocator, &reader));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_writer_create(&allocator, &writer));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_create(&allocator, &doc));
    CHECK(reader == NULL && writer == NULL && doc == NULL);
    allocator = allocator_of(&c);
    allocator.allocate = NULL;
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_reader_create(&allocator, &reader));
    CHECK_OK(x3pio_reader_create(NULL, &reader));
    x3pio_reader_free(reader);
    CHECK_OK(x3pio_reader_create(x3pio_default_allocator(), &reader));
    x3pio_reader_free(reader);
    CHECK(c.calls == 0);

    /* The allocator is copied: it need not outlive the call. */
    {
        x3pio_allocator temporary = allocator_of(&c);
        CHECK_OK(x3pio_document_create(&temporary, &doc));
        memset(&temporary, 0, sizeof temporary);
    }
    CHECK_OK(x3pio_document_set_comment(doc, "still works"));
    CHECK_STRING(x3pio_document_comment(doc), "still works");
    x3pio_document_free(doc);
    CHECK(c.live == 0);
}

/* A document copied into one of another allocator is the property of that
 * allocator. */
static void test_copy_between_allocators(void)
{
    static counter first;
    static counter second;
    reset(&first, 0);
    reset(&second, 0);
    const x3pio_allocator a = allocator_of(&first);
    const x3pio_allocator b = allocator_of(&second);
    x3pio_document *from = NULL;
    x3pio_document *to = NULL;
    CHECK_OK(x3pio_document_create(&a, &from));
    CHECK_OK(x3pio_document_create(&b, &to));
    CHECK_OK(x3pio_document_set_creator(from, "someone"));
    CHECK_OK(x3pio_document_copy(to, from));
    x3pio_document_free(from);
    CHECK(first.live == 0 && first.foreign_releases == 0);
    CHECK_STRING(x3pio_document_creator(to), "someone");
    x3pio_document_free(to);
    CHECK(second.live == 0 && second.foreign_releases == 0);
    CHECK(first.calls > 0 && second.calls > 0);
}

/* Reads a cloud of `count` points from a file in text in pieces of odd sizes,
 * with a reader that may hold `budget` bytes of points, and returns the most
 * memory the reader held. */
static size_t read_text_cloud(size_t count, size_t budget)
{
    const size_t COUNT = count;
    double *x = malloc(COUNT * sizeof *x);
    double *y = malloc(COUNT * sizeof *y);
    double *z = malloc(COUNT * sizeof *z);
    for (size_t i = 0; i < COUNT; ++i)
    {
        x[i] = (double)(float)(1e-3 * sin(0.01 * (double)i));
        y[i] = (double)(float)(1e-3 * cos(0.01 * (double)i));
        z[i] = (double)(float)(1e-6 * (double)(i % 977));
    }
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(NULL, &doc));
    CHECK_OK(x3pio_document_set_point_cloud(doc, COUNT, X3PIO_TYPE_FLOAT, 1.0));
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(NULL, &writer));
    CHECK_OK(x3pio_writer_set_storage(writer, X3PIO_STORAGE_TEXT));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    x3pio_const_fields from;
    x3pio_const_fields_init(&from);
    from.x = (x3pio_const_field){x, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    from.y = (x3pio_const_field){y, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    from.z = (x3pio_const_field){z, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    CHECK_OK(x3pio_write_points(writer, &from, COUNT));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);
    x3pio_document_free(doc);

    static counter c;
    reset(&c, 0);
    const x3pio_allocator allocator = allocator_of(&c);
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&allocator, &reader));
    CHECK_OK(x3pio_reader_set_text_memory(reader, budget));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    const size_t calls_after_open = c.calls;

    /* Pieces that do not line up with the chunks of the library or the parts
     * of the file. */
    double *rx = malloc(1500 * sizeof *rx);
    double *ry = malloc(1500 * sizeof *ry);
    double *rz = malloc(1500 * sizeof *rz);
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.x = (x3pio_field){rx, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    into.y = (x3pio_field){ry, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    into.z = (x3pio_field){rz, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    size_t done = 0;
    size_t piece = 1;
    while (done < COUNT)
    {
        const size_t n = COUNT - done < piece ? COUNT - done : piece;
        CHECK_OK(x3pio_read_points(reader, &into, n));
        for (size_t i = 0; i < n; ++i)
        {
            CHECK(rx[i] == x[done + i] && ry[i] == y[done + i]
                  && rz[i] == z[done + i]);
        }
        done += n;
        piece = piece * 3 % 1499 + 1;
    }
    CHECK(c.calls == calls_after_open);
    const size_t peak = c.peak_bytes;
    x3pio_reader_free(reader);
    CHECK(c.live == 0);
    free(rx);
    free(ry);
    free(rz);
    free(x);
    free(y);
    free(z);
    x3pio_buffer_free(&bytes);
    return peak;
}

/* The points of a file in text are held while they fit the budget of the
 * reader, so that the file is parsed once. A file with more points is parsed
 * again as the rows are asked for: the memory of the reader then does not
 * depend on the size of the file. Either way the transfer of rows allocates
 * nothing, however many parts of the file the parser goes through. */
static void test_text_in_parts(void)
{
    const size_t megabyte = (size_t)1 << 20U;
    /* Always parsed again. */
    const size_t small = read_text_cloud(12000, 0);
    const size_t large = read_text_cloud(120000, 0);
    CHECK(large < small + 65536);
    /* The points of the large file as numbers alone would take 2.9 megabytes.
     */
    CHECK(large < 2 * megabyte);

    /* Held: the memory follows the size of the file. */
    const size_t held = read_text_cloud(120000, 64 * megabyte);
    CHECK(held > large + 2 * megabyte);

    /* The budget decides: 12000 points fit 1 MiB, 120000 do not. */
    const size_t fits = read_text_cloud(12000, megabyte);
    const size_t does_not_fit = read_text_cloud(120000, megabyte);
    CHECK(fits == read_text_cloud(12000, 64 * megabyte));
    CHECK(does_not_fit < 2 * megabyte);
}

int main(void)
{
    test_balanced();
    test_failures();
    test_arguments();
    test_copy_between_allocators();
    test_text_in_parts();
    return finish_tests();
}
