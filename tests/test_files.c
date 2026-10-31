/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Files on disk and in memory, the files of vendors, the compression of the
 * container, and the states of the handles. */

#include "helpers.h"

#include <x3pio/file.h>

#include <stdint.h>
#include <string.h>

static x3pio_document *base_document(x3pio_revision revision)
{
    x3pio_document *d = NULL;
    CHECK_OK(x3pio_document_create(&d));
    CHECK_OK(x3pio_document_set_revision(d, revision));
    CHECK_OK(x3pio_document_set_surface(d, 4, 3, 1e-6, 1e-6, X3PIO_TYPE_FLOAT,
                                        1.0, 1));
    return d;
}

static const double heights[12] = {1e-7, 2e-7, 3e-7, 4e-7, 5e-7,   6e-7,
                                   7e-7, 8e-7, 9e-7, 1e-6, 1.1e-6, 1.2e-6};

static void finish_with_heights(x3pio_writer *writer)
{
    CHECK_OK(x3pio_write_heights(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE),
                                 heights));
    CHECK_OK(x3pio_end_file(writer));
}

static void check_heights(x3pio_reader *reader)
{
    double back[12];
    CHECK_OK(x3pio_read_heights(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE),
                                back));
    for (size_t i = 0; i < 12; ++i)
    {
        CHECK_NEAR(back[i], heights[i], 1e-12);
    }
}

/* ---- disk and memory
 * ---------------------------------------------------------- */

static void test_disk(void)
{
    const char *path = "test_files_h\xC3\xA4llo.x3p"; /* a path in UTF-8 */
    remove(path);
    x3pio_document *doc = base_document(X3PIO_REVISION_AMD1);
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(x3pio_create_file(writer, path, doc));
    finish_with_heights(writer);
    x3pio_writer_free(writer);

    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_file(reader, path));
    check_heights(reader);
    x3pio_reader_free(reader);

    /* The same file written to memory is the same bytes. */
    x3pio_buffer on_disk;
    x3pio_buffer in_memory;
    x3pio_buffer_init(&on_disk, x3pio_default_allocator());
    x3pio_buffer_init(&in_memory, x3pio_default_allocator());
    CHECK(read_whole_file(path, &on_disk));
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(create_in_buffer(writer, doc, &in_memory));
    finish_with_heights(writer);
    x3pio_writer_free(writer);
    CHECK(on_disk.size == in_memory.size
          && memcmp(on_disk.data, in_memory.data, on_disk.size) == 0);

    /* A file that cannot be created creates nothing, and the writer can try
     * again. */
    const char *copy = "test_files_copy.x3p";
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_STATUS(X3PIO_ERR_IO,
                 x3pio_create_file(writer, "no/such/folder/file.x3p", doc));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "The folder of the file does not exist");
    CHECK_OK(x3pio_create_file(writer, copy, doc));
    finish_with_heights(writer);
    x3pio_writer_free(writer);
    remove(copy);

    /* A document that cannot be written creates no file. */
    x3pio_document *empty = NULL;
    CHECK_OK(x3pio_document_create(&empty));
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_create_file(writer, copy, empty));
    FILE *probe = fopen(copy, "rb");
    CHECK(probe == NULL);
    if (probe != NULL)
    {
        fclose(probe);
    }
    CHECK_OK(x3pio_create_file(writer, copy, doc));
    x3pio_writer_free(writer);
    remove(copy);
    x3pio_document_free(empty);

    /* What cannot be opened. */
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_STATUS(X3PIO_ERR_IO, x3pio_open_file(reader, "does/not/exist.x3p"));
    CHECK_STATUS(X3PIO_ERR_IO, x3pio_open_file(reader, "."));
    FILE *text = fopen(copy, "wb");
    if (text != NULL)
    {
        fputs("this is no zip file at all\n", text);
        fclose(text);
    }
    CHECK_STATUS(X3PIO_ERR_FORMAT, x3pio_open_file(reader, copy));
    text = fopen(copy, "wb");
    if (text != NULL)
    {
        fclose(text);
    }
    CHECK_STATUS(X3PIO_ERR_FORMAT, x3pio_open_file(reader, copy));
    remove(copy);
    /* The reader opens a good file after these failures. */
    CHECK_OK(x3pio_open_file(reader, path));
    check_heights(reader);
    x3pio_reader_free(reader);

    x3pio_buffer_free(&on_disk);
    x3pio_buffer_free(&in_memory);
    x3pio_document_free(doc);
    remove(path);
}

/* A file in memory: the writer holds the bytes until it is freed, they are the
 * bytes of the file on disk, and they open as they are. */
static void test_memory_writer(void)
{
    x3pio_document *doc = base_document(X3PIO_REVISION_AMD1);
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    const void *data = NULL;
    size_t size = 0;
    /* Not started, and not ended. */
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_writer_memory(writer, &data, &size));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_create_memory(NULL, doc));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_create_memory(writer, NULL));
    CHECK_OK(x3pio_create_memory(writer, doc));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_create_memory(writer, doc));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_writer_memory(writer, &data, &size));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_writer_memory(NULL, &data, &size));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_writer_memory(writer, NULL, &size));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_writer_memory(writer, &data, NULL));
    finish_with_heights(writer);
    CHECK_OK(x3pio_writer_memory(writer, &data, &size));
    CHECK(data != NULL && size > 0);

    /* The bytes are those of a file written to disk. */
    const char *path = "test_files_memory.x3p";
    x3pio_writer *to_disk = NULL;
    CHECK_OK(x3pio_writer_create(&to_disk));
    CHECK_OK(x3pio_create_file(to_disk, path, doc));
    finish_with_heights(to_disk);
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_writer_memory(to_disk, &data, &size));
    x3pio_writer_free(to_disk);
    x3pio_buffer on_disk;
    x3pio_buffer_init(&on_disk, x3pio_default_allocator());
    CHECK(read_whole_file(path, &on_disk));
    remove(path);
    CHECK_OK(x3pio_writer_memory(writer, &data, &size));
    CHECK(size == on_disk.size && memcmp(data, on_disk.data, size) == 0);

    /* And they open as they are, in memory, while the writer lives. */
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, data, size));
    check_heights(reader);
    x3pio_reader_free(reader);

    /* A file that is cut short is no file. */
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_STATUS(X3PIO_ERR_FORMAT, x3pio_open_memory(reader, data, size / 2));
    x3pio_reader_free(reader);
    x3pio_writer_free(writer);
    x3pio_buffer_free(&on_disk);
    x3pio_document_free(doc);
}

/* ---- the files of vendors
 * --------------------------------------------------------- */

static const unsigned char vendor_bytes[] =
    "settings of the instrument\nlevel=3\n";

static void test_vendor_files_amd1(void)
{
    x3pio_document *doc = base_document(X3PIO_REVISION_AMD1);
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    const x3pio_compression store = {X3PIO_COMPRESS_STORE, 0};
    CHECK_OK(x3pio_add_vendor_file(writer, "http://www.vendor.com/my/a.xml",
                                   NULL, vendor_bytes, sizeof vendor_bytes,
                                   NULL));
    CHECK_OK(x3pio_add_vendor_file(writer, "http://other.org/thumb.png",
                                   "images/thumb.png", "PNG", 3, &store));
    CHECK_OK(x3pio_add_vendor_file(writer, "http://empty.net/nothing", NULL,
                                   NULL, 0, NULL));
    /* What cannot be added. */
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://www.vendor.com/my/a.xml",
                                       NULL, "x", 1, NULL));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "The vendor id is registered already");
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_add_vendor_file(writer, "not a uri",
                                                           NULL, "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://a.b/c", "main.xml", "x",
                                       1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://a.b/c",
                                       "bindata\\data.bin", "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://a.b/c", "../escape", "x",
                                       1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://a.b/c",
                                       "images/thumb.png", "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(
                     writer, "http://a.b/c", "z", "x", 1,
                     &(x3pio_compression){X3PIO_COMPRESS_DEFLATE, 12}));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, NULL, "z", "x", 1, NULL));
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_add_vendor_file(writer, "http://a.b/c", "z", NULL, 1, NULL));
    finish_with_heights(writer);
    CHECK_STATUS(
        X3PIO_ERR_STATE,
        x3pio_add_vendor_file(writer, "http://a.b/late", NULL, "x", 1, NULL));
    x3pio_writer_free(writer);

    archive a = {0};
    CHECK(archive_unzip(&a, bytes.data, bytes.size));
    member *first = archive_find(&a, "www\\vendor\\com\\my\\a.xml");
    CHECK(first != NULL && first->data.size == sizeof vendor_bytes
          && memcmp(first->data.data, vendor_bytes, sizeof vendor_bytes) == 0);
    member *thumb = archive_find(&a, "images/thumb.png");
    CHECK(thumb != NULL && thumb->method == 0);
    CHECK(first != NULL && first->method == 8);
    CHECK(archive_find(&a, "empty\\net\\nothing") != NULL);
    char error[100];
    member *main = archive_find(&a, "main.xml");
    CHECK(main != NULL
          && xsd_validate("iso25178-72-fdam1.xsd", main->data.data,
                          main->data.size, error, sizeof error));
    archive_free(&a);

    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    const x3pio_document *read = x3pio_reader_document(reader);
    CHECK(x3pio_document_vendor_count(read) == 3);
    CHECK_STRING(x3pio_document_vendor_id(read, 0),
                 "http://www.vendor.com/my/a.xml");
    CHECK(x3pio_reader_vendor_file_count(reader) == 3);
    CHECK_STRING(x3pio_reader_vendor_file_id(reader, 0),
                 "http://www.vendor.com/my/a.xml");
    CHECK_STRING(x3pio_reader_vendor_file_member(reader, 0),
                 "www/vendor/com/my/a.xml");
    /* An identifier whose file has another name has no file. */
    CHECK(x3pio_reader_vendor_file_id(reader, 3) == NULL);
    CHECK(x3pio_reader_vendor_file_member(reader, 3) == NULL);
    CHECK(x3pio_reader_vendor_file_id(NULL, 0) == NULL);
    CHECK(x3pio_reader_vendor_file_count(NULL) == 0);
    CHECK_STRING(x3pio_reader_vendor_file_id(reader, 1),
                 "http://other.org/thumb.png");
    CHECK(x3pio_reader_vendor_file_member(reader, 1) == NULL);

    /* Reading a member: the size first, then the bytes. */
    size_t size = 0;
    CHECK_STATUS(
        X3PIO_ERR_RANGE,
        x3pio_read_member(reader, "www/vendor/com/my/a.xml", NULL, 0, &size));
    CHECK(size == sizeof vendor_bytes);
    unsigned char content[64];
    CHECK_OK(x3pio_read_member(reader, "www/vendor/com/my/a.xml", content,
                               sizeof content, &size));
    CHECK(size == sizeof vendor_bytes
          && memcmp(content, vendor_bytes, size) == 0);
    CHECK_OK(x3pio_read_member(reader, "www\\vendor\\com\\my\\a.xml", content,
                               sizeof content, &size));
    CHECK_OK(x3pio_read_member(reader, "images/thumb.png", content,
                               sizeof content, &size));
    CHECK(size == 3 && memcmp(content, "PNG", 3) == 0);
    CHECK_OK(x3pio_read_member(reader, "empty/net/nothing", content,
                               sizeof content, &size));
    CHECK(size == 0);
    CHECK_STATUS(X3PIO_ERR_RANGE,
                 x3pio_read_member(reader, "www/vendor/com/my/a.xml", content,
                                   4, &size));
    CHECK_STATUS(X3PIO_ERR_IO,
                 x3pio_read_member(reader, "no/such/member", content,
                                   sizeof content, &size));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_read_member(reader, "../main.xml", content,
                                   sizeof content, &size));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_member(reader, NULL, content,
                                                       sizeof content, &size));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_read_member(reader, "main.xml", NULL, 5, &size));
    /* The original main.xml is a member too, and the rows still read. */
    CHECK_STATUS(X3PIO_ERR_RANGE,
                 x3pio_read_member(reader, "main.xml", NULL, 0, &size));
    CHECK(size > 500);
    check_heights(reader);
    x3pio_reader_free(reader);
    x3pio_document_free(doc);
    x3pio_buffer_free(&bytes);
}

static void test_vendor_files_2017(void)
{
    x3pio_document *doc = base_document(X3PIO_REVISION_2017);
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    /* The name in the container is required, and one identifier serves all. */
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://www.vendor.com/x", NULL,
                                       "x", 1, NULL));
    CHECK_STRING(
        x3pio_writer_error_message(writer),
        "ISO 25178-72:2017 needs the name of the file in the container");
    CHECK_OK(x3pio_add_vendor_file(writer, "http://www.vendor.com/x",
                                   "settings.ini", "a=1\n", 4, NULL));
    CHECK_OK(x3pio_add_vendor_file(writer, "http://www.vendor.com/x",
                                   "extra/data.bin", "01234", 5, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://other.org/y",
                                       "third.txt", "x", 1, NULL));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "ISO 25178-72:2017 allows one vendor id for all files");
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "", "third.txt", "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://www.vendor.com/x",
                                       "settings.ini", "x", 1, NULL));
    finish_with_heights(writer);
    x3pio_writer_free(writer);

    archive a = {0};
    CHECK(archive_unzip(&a, bytes.data, bytes.size));
    char error[100];
    member *main = archive_find(&a, "main.xml");
    CHECK(main != NULL
          && xsd_validate("iso25178-72-2017.xsd", main->data.data,
                          main->data.size, error, sizeof error));
    CHECK(archive_find(&a, "settings.ini") != NULL);
    archive_free(&a);

    /* Every file that is not part of the format belongs to the vendor. */
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    CHECK(x3pio_reader_vendor_file_count(reader) == 2);
    bool settings = false;
    bool extra = false;
    for (size_t i = 0; i < 2; ++i)
    {
        CHECK_STRING(x3pio_reader_vendor_file_id(reader, i),
                     "http://www.vendor.com/x");
        const char *name = x3pio_reader_vendor_file_member(reader, i);
        settings =
            settings || (name != NULL && strcmp(name, "settings.ini") == 0);
        extra = extra || (name != NULL && strcmp(name, "extra/data.bin") == 0);
    }
    CHECK(settings && extra);
    unsigned char content[8];
    size_t size = 0;
    CHECK_OK(x3pio_read_member(reader, "extra/data.bin", content,
                               sizeof content, &size));
    CHECK(size == 5 && memcmp(content, "01234", 5) == 0);
    x3pio_reader_free(reader);
    x3pio_document_free(doc);
    x3pio_buffer_free(&bytes);
}

/* ---- compression
 * --------------------------------------------------------------------- */

static size_t file_size_with(const x3pio_compression *compression, archive *a)
{
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(&doc));
    CHECK_OK(x3pio_document_set_surface(doc, 200, 200, 1e-6, 1e-6,
                                        X3PIO_TYPE_DOUBLE, 1.0, 1));
    double *z = malloc(200 * 200 * sizeof *z);
    for (size_t i = 0; i < 200 * 200; ++i)
    {
        z[i] = 1e-7 * (double)((i / 200) % 7);
    }
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    if (compression != NULL)
    {
        CHECK_OK(x3pio_writer_set_compression(writer, compression));
    }
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    CHECK_OK(
        x3pio_write_heights(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);
    const size_t size = bytes.size;
    if (a != NULL)
    {
        CHECK(archive_unzip(a, bytes.data, bytes.size));
    }
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);
    free(z);
    return size;
}

static void test_compression(void)
{
    archive stored = {0};
    archive deflated = {0};
    const x3pio_compression store = {X3PIO_COMPRESS_STORE, 0};
    const x3pio_compression fast = {X3PIO_COMPRESS_DEFLATE, 1};
    const x3pio_compression best = {X3PIO_COMPRESS_DEFLATE, 9};
    const size_t stored_size = file_size_with(&store, &stored);
    const size_t default_size = file_size_with(NULL, &deflated);
    const size_t fast_size = file_size_with(&fast, NULL);
    const size_t best_size = file_size_with(&best, NULL);
    for (size_t i = 0; i < stored.count; ++i)
    {
        CHECK(stored.items[i].method == 0);
    }
    for (size_t i = 0; i < deflated.count; ++i)
    {
        CHECK(deflated.items[i].method == 8);
    }
    CHECK(stored_size > 200 * 200 * 8);
    CHECK(default_size < stored_size / 4);
    CHECK(best_size <= fast_size);
    CHECK(best_size <= default_size + 16);
    archive_free(&stored);
    archive_free(&deflated);

    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_writer_set_compression(
                     writer, &(x3pio_compression){X3PIO_COMPRESS_DEFLATE, 10}));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_writer_set_compression(
                     writer, &(x3pio_compression){X3PIO_COMPRESS_DEFLATE, -2}));
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_writer_set_compression(
            writer, &(x3pio_compression){(x3pio_compression_method)5, 1}));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_writer_set_compression(writer, NULL));
    CHECK_OK(x3pio_writer_set_compression(
        writer, &(x3pio_compression){X3PIO_COMPRESS_DEFLATE, 0}));
    const x3pio_compression defaults = x3pio_compression_default();
    CHECK(defaults.method == X3PIO_COMPRESS_DEFLATE && defaults.level == -1);
    x3pio_writer_free(writer);
}

/* ---- the states of the handles
 * ------------------------------------------------------- */

static void test_reader_states(void)
{
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    file_spec spec = default_spec();
    make_base_file(&spec, &bytes);
    x3pio_reader *reader = NULL;
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_reader_create(NULL));
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_STRING(x3pio_reader_error_message(reader), "");
    CHECK_STRING(x3pio_reader_error_message(NULL), "");

    /* Nothing is read before a file is open. */
    double z[12];
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.z = (x3pio_field){z, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    CHECK(x3pio_reader_document(reader) == NULL);
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_read_points(reader, &into, 3));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_read_data(reader, &into, 3));
    CHECK_STATUS(
        X3PIO_ERR_STATE,
        x3pio_read_heights(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
    size_t size = 0;
    CHECK_STATUS(X3PIO_ERR_STATE,
                 x3pio_read_member(reader, "main.xml", NULL, 0, &size));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(NULL, &into, 3));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, NULL, 3));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_open_memory(NULL, bytes.data, bytes.size));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_open_memory(reader, NULL, 5));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_open_file(reader, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_reader_set_chunk(reader, 0));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_reader_set_chunk(reader, SIZE_MAX));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_reader_set_chunk(NULL, 10));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_reader_set_verify(NULL, true));

    /* An opening that fails leaves the reader new. */
    CHECK_STATUS(X3PIO_ERR_FORMAT, x3pio_open_memory(reader, "not a zip", 9));
    CHECK(x3pio_reader_document(reader) == NULL);
    CHECK_OK(x3pio_reader_set_chunk(reader, 5));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    CHECK(x3pio_reader_document(reader) != NULL);

    /* The options are fixed by the first file. */
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_reader_set_chunk(reader, 10));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_reader_set_verify(reader, false));
    CHECK_STATUS(X3PIO_ERR_STATE,
                 x3pio_open_memory(reader, bytes.data, bytes.size));
    CHECK_STRING(x3pio_reader_error_message(reader),
                 "The reader has opened a file already");

    /* A mistake of the arguments changes nothing; the rows are read in order.
     */
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, &into, 4));
    CHECK_OK(x3pio_read_points(reader, &into, 0));
    CHECK_OK(x3pio_read_points(reader, &into, 1));
    CHECK_STATUS(
        X3PIO_ERR_STATE,
        x3pio_read_heights(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
    CHECK_OK(x3pio_read_points(reader, &into, 2));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, &into, 1));
    x3pio_reader_free(reader);
    x3pio_reader_free(NULL);
    x3pio_buffer_free(&bytes);
}

static void test_writer_states(void)
{
    x3pio_document *doc = base_document(X3PIO_REVISION_AMD1);
    x3pio_writer *writer = NULL;
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_writer_create(NULL));
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_STRING(x3pio_writer_error_message(writer), "");
    CHECK_STRING(x3pio_writer_error_message(NULL), "");

    /* Nothing is written before a file is created. */
    double z[12] = {0};
    x3pio_const_fields from;
    x3pio_const_fields_init(&from);
    from.z = (x3pio_const_field){z, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_write_points(writer, &from, 3));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_write_data(writer, &from, 3));
    CHECK_STATUS(
        X3PIO_ERR_STATE,
        x3pio_write_heights(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_end_file(writer));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_add_vendor_file(writer, "http://a.b/c",
                                                        NULL, "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_end_file(NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_create_memory(writer, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_points(NULL, &from, 3));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_writer_set_chunk(writer, 0));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_writer_set_storage(writer, (x3pio_storage)9));

    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(x3pio_writer_set_chunk(writer, 5));
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    CHECK_STATUS(X3PIO_ERR_STATE, create_in_buffer(writer, doc, &bytes));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_writer_set_chunk(writer, 10));
    CHECK_STATUS(X3PIO_ERR_STATE,
                 x3pio_writer_set_storage(writer, X3PIO_STORAGE_TEXT));
    /* The document was copied: a change of it has no effect. */
    CHECK_OK(x3pio_document_set_surface(doc, 9, 9, 1.0, 1.0, X3PIO_TYPE_DOUBLE,
                                        1.0, 1));

    /* Rows are missing. */
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_end_file(writer));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "Not all points of the file were written");
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_points(writer, &from, 4));
    CHECK_OK(x3pio_write_points(writer, &from, 1));
    CHECK_STATUS(
        X3PIO_ERR_STATE,
        x3pio_write_heights(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
    /* Fields that do not fit the file. */
    x3pio_const_fields wrong;
    x3pio_const_fields_init(&wrong);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_points(writer, &wrong, 1));
    wrong.z = from.z;
    wrong.x = from.z;
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_points(writer, &wrong, 1));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "An axis that the file does not store cannot be bound");
    CHECK_OK(x3pio_write_points(writer, &from, 2));
    CHECK_OK(x3pio_end_file(writer));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_end_file(writer));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_write_points(writer, &from, 1));
    x3pio_writer_free(writer);
    x3pio_writer_free(NULL);

    /* A file that was written is read. */
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    CHECK(x3pio_document_columns(x3pio_reader_document(reader)) == 4);
    x3pio_reader_free(reader);
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);
}

/* The rows of a file that is damaged: an error ends the reading. */
static void test_damage(void)
{
    file_spec spec = default_spec();
    spec.compression = (x3pio_compression){X3PIO_COMPRESS_STORE, 0};
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    make_base_file(&spec, &bytes);
    archive a = {0};
    CHECK(archive_unzip(&a, bytes.data, bytes.size));

    /* A file of vendor with a pattern that is found in the zip file. */
    x3pio_document *doc = base_document(X3PIO_REVISION_AMD1);
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(x3pio_writer_set_compression(writer, &spec.compression));
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    static const unsigned char pattern[] = "VENDOR-PATTERN-0123456789";
    CHECK_OK(x3pio_add_vendor_file(writer, "http://www.vendor.com/pattern.txt",
                                   NULL, pattern, sizeof pattern, NULL));
    finish_with_heights(writer);
    x3pio_writer_free(writer);
    x3pio_document_free(doc);

    /* A byte of the file of the vendor is changed: only that member is lost. */
    x3pio_buffer damaged;
    x3pio_buffer_init(&damaged, x3pio_default_allocator());
    x3pio_buffer_append(&damaged, bytes.data, bytes.size);
    unsigned char *hit = NULL;
    for (size_t i = 0; i + sizeof pattern <= damaged.size; ++i)
    {
        if (memcmp(damaged.data + i, pattern, sizeof pattern) == 0)
        {
            hit = damaged.data + i;
            break;
        }
    }
    CHECK(hit != NULL);
    if (hit != NULL)
    {
        hit[5] ^= 0x55;
        x3pio_reader *reader = NULL;
        CHECK_OK(x3pio_reader_create(&reader));
        CHECK_OK(x3pio_open_memory(reader, damaged.data, damaged.size));
        check_heights(reader);
        unsigned char content[64];
        size_t size = 0;
        CHECK_STATUS(X3PIO_ERR_FORMAT,
                     x3pio_read_member(reader, "www/vendor/com/pattern.txt",
                                       content, sizeof content, &size));
        x3pio_reader_free(reader);
    }

    /* A byte of the points is changed: the vendor file is still read. */
    x3pio_buffer_free(&damaged);
    x3pio_buffer_init(&damaged, x3pio_default_allocator());
    x3pio_buffer_append(&damaged, bytes.data, bytes.size);
    /* The points are floats of the machine in the file. */
    const float one = (float)heights[0];
    unsigned char needle[4];
    memcpy(needle, &one, 4);
    unsigned char *where = NULL;
    for (size_t i = 0; i + 4 <= damaged.size; ++i)
    {
        if (memcmp(damaged.data + i, needle, 4) == 0)
        {
            where = damaged.data + i;
            break;
        }
    }
    CHECK(where != NULL);
    if (where != NULL)
    {
        where[0] ^= 0x01;
        x3pio_reader *reader = NULL;
        CHECK_OK(x3pio_reader_create(&reader));
        CHECK_OK(x3pio_open_memory(reader, damaged.data, damaged.size));
        unsigned char content[64];
        size_t size = 0;
        CHECK_OK(x3pio_read_member(reader, "www/vendor/com/pattern.txt",
                                   content, sizeof content, &size));
        CHECK(size == sizeof pattern);
        double back[12];
        const x3pio_status status = x3pio_read_heights(
            reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), back);
        CHECK(status == X3PIO_ERR_FORMAT || status == X3PIO_ERR_CHECKSUM);
        /* An error ends the reading. */
        CHECK_STATUS(X3PIO_ERR_STATE,
                     x3pio_read_heights(
                         reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), back));
        x3pio_reader_free(reader);
    }
    x3pio_buffer_free(&damaged);
    archive_free(&a);
    x3pio_buffer_free(&bytes);
}

int main(void)
{
    test_disk();
    test_memory_writer();
    test_vendor_files_amd1();
    test_vendor_files_2017();
    test_compression();
    test_reader_states();
    test_writer_states();
    test_damage();
    return finish_tests();
}
