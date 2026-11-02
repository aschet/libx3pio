/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Files on disk and in memory, the files of vendors, the compression of the
 * container, and the states of the handles. */

#include "byte_order.h"
#include "helpers.h"

#include <x3pio/file.h>

#include <stdint.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

/* Removes a file by a path in UTF-8. On Windows the path of remove() is in the
 * code page of the system, so the path is converted first. */
static void remove_file(const char *path)
{
#ifdef _WIN32
    wchar_t wide[MAX_PATH];
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, MAX_PATH) > 0)
    {
        (void)_wremove(wide);
    }
#else
    (void)remove(path);
#endif
}

static x3pio_document *base_document(x3pio_revision revision)
{
    x3pio_document *d = NULL;
    CHECK_OK(x3pio_document_create(&d));
    CHECK_OK(x3pio_document_set_revision(d, revision));
    CHECK_OK(x3pio_document_set_feature_surface(d, 4, 3, 1e-6, 1e-6,
                                                X3PIO_TYPE_FLOAT, 1.0, 1));
    return d;
}

static const double heights[12] = {1e-7, 2e-7, 3e-7, 4e-7, 5e-7,   6e-7,
                                   7e-7, 8e-7, 9e-7, 1e-6, 1.1e-6, 1.2e-6};

static void finish_with_heights(x3pio_writer *writer)
{
    CHECK_OK(
        x3pio_write_z(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), heights));
    CHECK_OK(x3pio_end_file(writer));
}

static void check_heights(x3pio_reader *reader)
{
    double back[12];
    CHECK_OK(
        x3pio_read_z(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), back));
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
    remove_file(path);
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
    remove_file(copy);

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
    remove_file(copy);
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
    remove_file(copy);
    /* The reader opens a good file after these failures. */
    CHECK_OK(x3pio_open_file(reader, path));
    check_heights(reader);
    x3pio_reader_free(reader);

    x3pio_buffer_free(&on_disk);
    x3pio_buffer_free(&in_memory);
    x3pio_document_free(doc);
    remove_file(path);
}

#ifdef _WIN32
/* The same files by a path of wide characters, with letters outside the code
 * page and a character that needs a surrogate pair. */
static void test_wide_paths(void)
{
    static const wchar_t path[] = L"test_files_wäö \U0001F600.x3p";
    static const char utf8[] =
        "test_files_w\xC3\xA4\xC3\xB6 \xF0\x9F\x98\x80.x3p";
    (void)_wremove(path);
    x3pio_document *doc = base_document(X3PIO_REVISION_AMD1);
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(x3pio_create_wfile(writer, path, doc));
    finish_with_heights(writer);
    x3pio_writer_free(writer);

    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_wfile(reader, path));
    check_heights(reader);
    x3pio_reader_free(reader);

    /* The path in UTF-8 reaches the same file. */
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_file(reader, utf8));
    check_heights(reader);
    x3pio_reader_free(reader);

    /* A surrogate without a partner is no text. */
    static const wchar_t lone[] = L"test_files_\xD800.x3p";
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_open_wfile(reader, lone));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_open_wfile(NULL, path));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_open_wfile(reader, NULL));
    CHECK_STATUS(X3PIO_ERR_IO, x3pio_open_wfile(reader, L"no such file.x3p"));
    x3pio_reader_free(reader);
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_create_wfile(writer, lone, doc));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_create_wfile(NULL, path, doc));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_create_wfile(writer, NULL, doc));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_create_wfile(writer, path, NULL));
    x3pio_writer_free(writer);
    (void)_wremove(path);
    x3pio_document_free(doc);
}
#endif

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
    remove_file(path);
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

/* A member that is far below 4 GiB has the plain header of a zip file, so that
 * a reader that does not know Zip64 or the data descriptor reads it: the
 * version is 2.0, the sizes and the checksum are in the header, and no field of
 * extra data holds Zip64 sizes. */
static void test_plain_headers(void)
{
    x3pio_document *doc = base_document(X3PIO_REVISION_AMD1);
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(x3pio_create_memory(writer, doc));
    finish_with_heights(writer);
    const void *data = NULL;
    size_t size = 0;
    CHECK_OK(x3pio_writer_memory(writer, &data, &size));
    const unsigned char *bytes = data;
    size_t headers = 0;
    for (size_t at = 0; at + 30 <= size; ++at)
    {
        if (memcmp(bytes + at, "PK\3\4", 4) != 0)
        {
            continue;
        }
        ++headers;
        const unsigned version = bytes[at + 4] | (unsigned)bytes[at + 5] << 8U;
        const unsigned flags = bytes[at + 6] | (unsigned)bytes[at + 7] << 8U;
        const size_t name = bytes[at + 26] | (size_t)bytes[at + 27] << 8U;
        const size_t extra = bytes[at + 28] | (size_t)bytes[at + 29] << 8U;
        CHECK(version == 20);
        CHECK((flags & 8U) == 0);
        size_t field = at + 30 + name;
        while (field + 4 <= at + 30 + name + extra)
        {
            const unsigned id = bytes[field] | (unsigned)bytes[field + 1] << 8U;
            CHECK(id != 1);
            field += 4 + (bytes[field + 2] | (size_t)bytes[field + 3] << 8U);
        }
        /* The compressed size is in the header, not behind the bytes. */
        CHECK(bytes[at + 18] != 0 || bytes[at + 19] != 0);
    }
    CHECK(headers >= 3);
    x3pio_writer_free(writer);
    x3pio_document_free(doc);
}

/* ---- the files of vendors
 * --------------------------------------------------------- */

static const unsigned char vendor_bytes[] =
    "settings of the instrument\nlevel=3\n";

/* The additions that no revision allows. */
static void check_refusals(x3pio_writer *writer)
{
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(NULL, "http://a.b", "c", "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, NULL, "c", "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_add_vendor_file(writer, "http://a.b",
                                                           NULL, "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_add_vendor_file(writer, "http://a.b",
                                                           "c", NULL, 1, NULL));
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_add_vendor_file(writer, "http://a.b", "../escape", "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(
                     writer, "http://a.b", "z", "x", 1,
                     &(x3pio_compression){X3PIO_COMPRESS_DEFLATE, 12}));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file_by_id(writer, NULL, "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_add_vendor_id(writer, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_add_vendor_id(writer, "  "));
}

static void test_vendor_files_amd1(void)
{
    x3pio_document *doc = base_document(X3PIO_REVISION_AMD1);
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    const x3pio_compression store = {X3PIO_COMPRESS_STORE, 0};
    /* The identifier of a file is the vendor, a slash and the path. */
    CHECK_OK(x3pio_add_vendor_file(writer, "http://www.vendor.com", "my/a.xml",
                                   vendor_bytes, sizeof vendor_bytes, NULL));
    /* A slash at the end of the vendor does not matter. */
    CHECK_OK(x3pio_add_vendor_file(writer, "http://other.org/", "thumb.png",
                                   "PNG", 3, &store));
    /* An identifier in full serves the same, as a reader reports it. */
    CHECK_OK(x3pio_add_vendor_file_by_id(writer, "http://empty.net/nothing",
                                         NULL, 0, NULL));
    /* An identifier without a file, such as a bare domain name. */
    CHECK_OK(x3pio_add_vendor_id(writer, "http://nofile.org"));
    /* An identifier that has no file yet takes it later. */
    CHECK_OK(x3pio_add_vendor_id(writer, "http://late.net/f.txt"));
    CHECK_OK(x3pio_add_vendor_file_by_id(writer, "http://late.net/f.txt", "L",
                                         1, NULL));
    check_refusals(writer);
    /* What only Amd 1 refuses. */
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://www.vendor.com",
                                       "my/a.xml", "x", 1, NULL));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "The vendor id is registered already");
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_id(writer, "http://nofile.org"));
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_add_vendor_file(writer, "http://a..b", "c", "x", 1, NULL));
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_add_vendor_file_by_id(writer, "http://host.net", "x", 1, NULL));
    /* A domain name has no user name and no port. */
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_add_vendor_file(writer, "http://host:8080", "c", "x", 1, NULL));
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_add_vendor_file(writer, "http://user@host", "c", "x", 1, NULL));
    /* A name is a file or a folder, not both. */
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://www.vendor.com", "my",
                                       "x", 1, NULL));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "The name would be both a file and a folder of the container");
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://www.vendor.com",
                                       "my/a.xml/b", "x", 1, NULL));
    finish_with_heights(writer);
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_add_vendor_file(writer, "http://a.b",
                                                        "late", "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_add_vendor_file_by_id(
                                      writer, "http://a.b/late", "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_add_vendor_id(writer, "http://a.b"));
    x3pio_writer_free(writer);

    archive a = {0};
    CHECK(archive_unzip(&a, bytes.data, bytes.size));
    member *first = archive_find(&a, "www\\vendor\\com\\my\\a.xml");
    CHECK(first != NULL && first->data.size == sizeof vendor_bytes
          && memcmp(first->data.data, vendor_bytes, sizeof vendor_bytes) == 0);
    member *thumb = archive_find(&a, "other\\org\\thumb.png");
    CHECK(thumb != NULL && thumb->method == 0);
    CHECK(first != NULL && first->method == 8);
    CHECK(archive_find(&a, "empty\\net\\nothing") != NULL);
    CHECK(archive_find(&a, "late\\net\\f.txt") != NULL);
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
    CHECK(x3pio_document_vendor_count(read) == 5);
    CHECK_STRING(x3pio_document_vendor_id(read, 0),
                 "http://www.vendor.com/my/a.xml");
    CHECK(x3pio_reader_vendor_file_count(reader) == 5);
    CHECK_STRING(x3pio_reader_vendor_file_id(reader, 0),
                 "http://www.vendor.com/my/a.xml");
    CHECK_STRING(x3pio_reader_vendor_file_member(reader, 0),
                 "www/vendor/com/my/a.xml");
    CHECK_STRING(x3pio_reader_vendor_file_id(reader, 1),
                 "http://other.org/thumb.png");
    CHECK_STRING(x3pio_reader_vendor_file_member(reader, 1),
                 "other/org/thumb.png");
    /* An identifier that names no file of the container has no member. */
    CHECK_STRING(x3pio_reader_vendor_file_id(reader, 3), "http://nofile.org");
    CHECK(x3pio_reader_vendor_file_member(reader, 3) == NULL);
    CHECK(x3pio_reader_vendor_file_id(reader, 5) == NULL);
    CHECK(x3pio_reader_vendor_file_member(reader, 5) == NULL);
    CHECK(x3pio_reader_vendor_file_id(NULL, 0) == NULL);
    CHECK(x3pio_reader_vendor_file_count(NULL) == 0);

    /* The files of one vendor, or of one folder of it. */
    size_t at = 0;
    CHECK_OK(x3pio_reader_find_vendor_file(reader, "https://www.vendor.com", 0,
                                           &at));
    CHECK(at == 0);
    CHECK_OK(
        x3pio_reader_find_vendor_file(reader, "www.vendor.com", at + 1, &at));
    CHECK(at == x3pio_reader_vendor_file_count(reader));
    CHECK_OK(
        x3pio_reader_find_vendor_file(reader, "http://other.org/", 0, &at));
    CHECK(at == 1);
    CHECK_OK(x3pio_reader_find_vendor_file(reader, "http://www.vendor.com/my",
                                           0, &at));
    CHECK(at == 0);
    CHECK_OK(x3pio_reader_find_vendor_file(reader, "http://www.vendor.com/m", 0,
                                           &at));
    CHECK(at == x3pio_reader_vendor_file_count(reader));
    /* An identifier without a file is found too. */
    CHECK_OK(
        x3pio_reader_find_vendor_file(reader, "http://nofile.org", 0, &at));
    CHECK(at == 3 && x3pio_reader_vendor_file_member(reader, at) == NULL);
    CHECK_OK(
        x3pio_reader_find_vendor_file(reader, "http://nowhere.net", 0, &at));
    CHECK(at == 5);
    CHECK_OK(
        x3pio_reader_find_vendor_file(reader, "http://other.org", 99, &at));
    CHECK(at == 5);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_reader_find_vendor_file(reader, "http:///x", 0, &at));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_reader_find_vendor_file(reader, NULL, 0, &at));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_reader_find_vendor_file(reader, "a.b", 0, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_reader_find_vendor_file(NULL, "a.b", 0, &at));

    /* A file by its identifier: the size first, then the bytes. */
    size_t size = 0;
    CHECK_STATUS(X3PIO_ERR_RANGE,
                 x3pio_read_vendor_file(
                     reader, "http://www.vendor.com/my/a.xml", NULL, 0, &size));
    CHECK(size == sizeof vendor_bytes);
    unsigned char content[64];
    CHECK_OK(x3pio_read_vendor_file(reader, "http://www.vendor.com/my/a.xml",
                                    content, sizeof content, &size));
    CHECK(size == sizeof vendor_bytes
          && memcmp(content, vendor_bytes, size) == 0);
    CHECK_OK(x3pio_read_vendor_file(reader, "http://other.org/thumb.png",
                                    content, sizeof content, &size));
    CHECK(size == 3 && memcmp(content, "PNG", 3) == 0);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_read_vendor_file(reader, "http://nofile.org", content,
                                        sizeof content, &size));
    CHECK_STRING(x3pio_reader_error_message(reader),
                 "The vendor id names no file of the container");
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_read_vendor_file(reader, "http://not.registered/x",
                                        content, sizeof content, &size));
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_read_vendor_file(reader, NULL, content, sizeof content, &size));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_read_vendor_file(reader, "http://other.org/thumb.png",
                                        NULL, 5, &size));

    /* Reading a member: the size first, then the bytes. */
    CHECK_STATUS(
        X3PIO_ERR_RANGE,
        x3pio_read_member(reader, "www/vendor/com/my/a.xml", NULL, 0, &size));
    CHECK(size == sizeof vendor_bytes);
    CHECK_OK(x3pio_read_member(reader, "www/vendor/com/my/a.xml", content,
                               sizeof content, &size));
    CHECK(size == sizeof vendor_bytes
          && memcmp(content, vendor_bytes, size) == 0);
    CHECK_OK(x3pio_read_member(reader, "www\\vendor\\com\\my\\a.xml", content,
                               sizeof content, &size));
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
    /* The original main.xml is a member too, and the points still read. */
    CHECK_STATUS(X3PIO_ERR_RANGE,
                 x3pio_read_member(reader, "main.xml", NULL, 0, &size));
    CHECK(size > 500);
    check_heights(reader);
    x3pio_reader_free(reader);
    x3pio_document_free(doc);
    x3pio_buffer_free(&bytes);
}

/* Identifiers that name the same file share it if the content is the same, and
 * the reader never takes a file of the format for a file of a vendor. */
static void test_vendor_rules(void)
{
    x3pio_document *doc = base_document(X3PIO_REVISION_AMD1);
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    CHECK_OK(x3pio_add_vendor_file(writer, "http://a.b", "c", "same", 4, NULL));
    /* The scheme is not part of the name. */
    CHECK_OK(
        x3pio_add_vendor_file(writer, "https://a.b", "c", "same", 4, NULL));
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_add_vendor_file(writer, "ftp://a.b", "c", "other", 5, NULL));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "The container has a file of that name already");
    finish_with_heights(writer);
    x3pio_writer_free(writer);

    archive a = {0};
    CHECK(archive_unzip(&a, bytes.data, bytes.size));
    size_t files = 0;
    for (size_t i = 0; i < a.count; ++i)
    {
        const char *name = a.items[i].name;
        files += name[0] == 'a' && (name[1] == '\\' || name[1] == '/')
                         && name[2] == 'b'
                     ? 1U
                     : 0U;
    }
    CHECK(files == 1);
    archive_free(&a);
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    CHECK(x3pio_reader_vendor_file_count(reader) == 2);
    CHECK_STRING(x3pio_reader_vendor_file_member(reader, 0), "a/b/c");
    CHECK_STRING(x3pio_reader_vendor_file_member(reader, 1), "a/b/c");
    x3pio_reader_free(reader);
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);

    /* A file of the first revision whose points are text: a member with the
     * name of the binary file belongs to the format, not to the vendor. */
    doc = base_document(X3PIO_REVISION_2017);
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(x3pio_writer_set_storage(writer, X3PIO_STORAGE_TEXT));
    x3pio_buffer text;
    x3pio_buffer_init(&text, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &text));
    CHECK_OK(x3pio_add_vendor_file(writer, "http://www.vendor.com/x",
                                   "settings.ini", "a=1\n", 4, NULL));
    finish_with_heights(writer);
    x3pio_writer_free(writer);
    CHECK(archive_unzip(&a, text.data, text.size));
    archive_put(&a, "bindata/data.bin", "stale", 5);
    archive_put(&a, "bindata/valid.bin", "stale", 5);
    x3pio_buffer changed;
    x3pio_buffer_init(&changed, x3pio_default_allocator());
    CHECK(archive_zip(&a, &changed));
    archive_free(&a);
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, changed.data, changed.size));
    CHECK(x3pio_reader_vendor_file_count(reader) == 1);
    CHECK_STRING(x3pio_reader_vendor_file_member(reader, 0), "settings.ini");
    x3pio_reader_free(reader);
    x3pio_buffer_free(&changed);
    x3pio_buffer_free(&text);
    x3pio_document_free(doc);
}

static void test_vendor_files_2017(void)
{
    x3pio_document *doc = base_document(X3PIO_REVISION_2017);
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    /* By identifier needs the vendor, which the first file fixes. */
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_add_vendor_file_by_id(
            writer, "http://www.vendor.com/x/settings.ini", "x", 1, NULL));
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_add_vendor_file(writer, "  ", "settings.ini", "x", 1, NULL));
    /* The same call as in Amd 1: the vendor and the path, which is the name in
     * the container here. */
    CHECK_OK(x3pio_add_vendor_file(writer, "http://www.vendor.com/x",
                                   "settings.ini", "a=1\n", 4, NULL));
    CHECK_OK(x3pio_add_vendor_file(writer, "http://www.vendor.com/x/",
                                   "extra/data.bin", "01234", 5, NULL));
    CHECK_OK(x3pio_add_vendor_file_by_id(
        writer, "http://www.vendor.com/x/more.txt", "m", 1, NULL));
    /* The vendor itself, again. */
    CHECK_OK(x3pio_add_vendor_id(writer, "http://www.vendor.com/x"));
    check_refusals(writer);
    /* What only the first revision refuses: a second vendor. */
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://other.org/y",
                                       "third.txt", "x", 1, NULL));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "ISO 25178-72:2017 allows one vendor id for all files");
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file_by_id(writer, "http://other.org/y/z",
                                             "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_id(writer, "http://other.org/y"));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://www.vendor.com/x",
                                       "settings.ini", "x", 1, NULL));
    /* A name is a file or a folder, not both. */
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://www.vendor.com/x",
                                       "extra", "x", 1, NULL));
    /* The path is the name in the container here, so the files of the format
     * are taken. */
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://www.vendor.com/x",
                                       "main.xml", "x", 1, NULL));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "The name is one that the format uses");
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_add_vendor_file(writer, "http://www.vendor.com/x",
                                       "bindata\\data.bin", "x", 1, NULL));
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

    /* Every file that is not part of the format belongs to the vendor, and has
     * an identifier of its own: the vendor and the name. */
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    CHECK(x3pio_document_vendor_count(x3pio_reader_document(reader)) == 1);
    CHECK_STRING(x3pio_document_vendor_id(x3pio_reader_document(reader), 0),
                 "http://www.vendor.com/x");
    CHECK(x3pio_reader_vendor_file_count(reader) == 3);
    bool settings = false;
    bool extra = false;
    for (size_t i = 0; i < 3; ++i)
    {
        const char *id = x3pio_reader_vendor_file_id(reader, i);
        const char *name = x3pio_reader_vendor_file_member(reader, i);
        CHECK(name != NULL && strncmp(id, "http://www.vendor.com/x/", 24) == 0
              && strcmp(id + 24, name) == 0);
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
    /* The files of the vendor, or of a folder of it, as in Amd 1. */
    size_t found = 0;
    size_t matches = 0;
    for (size_t at = 0;;)
    {
        CHECK_OK(x3pio_reader_find_vendor_file(
            reader, "http://www.vendor.com/x", at, &found));
        if (found == x3pio_reader_vendor_file_count(reader))
        {
            break;
        }
        ++matches;
        at = found + 1;
    }
    CHECK(matches == 3);
    CHECK_OK(x3pio_reader_find_vendor_file(
        reader, "http://www.vendor.com/x/extra", 0, &found));
    CHECK(found < 3
          && strcmp(x3pio_reader_vendor_file_member(reader, found),
                    "extra/data.bin")
                 == 0);
    CHECK_OK(
        x3pio_reader_find_vendor_file(reader, "http://other.org", 0, &found));
    CHECK(found == 3);
    /* The identifier reads the file, as in Amd 1. */
    CHECK_OK(x3pio_read_vendor_file(reader,
                                    "http://www.vendor.com/x/extra/data.bin",
                                    content, sizeof content, &size));
    CHECK(size == 5 && memcmp(content, "01234", 5) == 0);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_read_vendor_file(reader, "http://www.vendor.com/x",
                                        content, sizeof content, &size));
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
    CHECK_OK(x3pio_document_set_feature_surface(doc, 200, 200, 1e-6, 1e-6,
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
    CHECK_OK(x3pio_write_z(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
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
    create_base_file(&spec, &bytes);
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
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_read_points(reader, &into, 12));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_read_data(reader, &into, 12));
    CHECK_STATUS(
        X3PIO_ERR_STATE,
        x3pio_read_z(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
    size_t size = 0;
    CHECK_STATUS(X3PIO_ERR_STATE,
                 x3pio_read_member(reader, "main.xml", NULL, 0, &size));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(NULL, &into, 12));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, NULL, 12));
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

    /* A mistake of the arguments changes nothing; the points are read in
     * order. A matrix is read in whole runs along u, and not beyond its end. */
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, &into, 16));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, &into, 5));
    CHECK_STRING(x3pio_reader_error_message(reader),
                 "A matrix is read in runs of SizeX points along u");
    CHECK_OK(x3pio_read_points(reader, &into, 0));
    CHECK_OK(x3pio_read_points(reader, &into, 4));
    CHECK_STATUS(
        X3PIO_ERR_STATE,
        x3pio_read_z(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
    CHECK_OK(x3pio_read_points(reader, &into, 8));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, &into, 4));
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
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_write_points(writer, &from, 12));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_write_data(writer, &from, 12));
    CHECK_STATUS(
        X3PIO_ERR_STATE,
        x3pio_write_z(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_end_file(writer));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_add_vendor_file(writer, "http://a.b",
                                                        "c", "x", 1, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_end_file(NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_create_memory(writer, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_points(NULL, &from, 12));
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
    CHECK_OK(x3pio_document_set_feature_surface(doc, 9, 9, 1.0, 1.0,
                                                X3PIO_TYPE_DOUBLE, 1.0, 1));

    /* Points are missing. */
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_end_file(writer));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "Not all points of the file were written");
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_points(writer, &from, 16));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_points(writer, &from, 5));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "A matrix is written in runs of SizeX points along u");
    CHECK_OK(x3pio_write_points(writer, &from, 4));
    CHECK_STATUS(
        X3PIO_ERR_STATE,
        x3pio_write_z(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
    /* Fields that do not fit the file. */
    x3pio_const_fields wrong;
    x3pio_const_fields_init(&wrong);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_points(writer, &wrong, 4));
    wrong.z = from.z;
    wrong.x = from.z;
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_points(writer, &wrong, 4));
    CHECK_STRING(x3pio_writer_error_message(writer),
                 "An axis that the file does not store cannot be bound");
    CHECK_OK(x3pio_write_points(writer, &from, 8));
    CHECK_OK(x3pio_end_file(writer));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_end_file(writer));
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_write_points(writer, &from, 4));
    x3pio_writer_free(writer);
    x3pio_writer_free(NULL);

    /* A file that was written is read. */
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    CHECK(x3pio_document_dimensions(x3pio_reader_document(reader)).size_x == 4);
    x3pio_reader_free(reader);
    x3pio_buffer_free(&bytes);
    x3pio_document_free(doc);
}

/* The points of a file that is damaged: an error ends the reading. */
static void test_damage(void)
{
    file_spec spec = default_spec();
    spec.compression = (x3pio_compression){X3PIO_COMPRESS_STORE, 0};
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    create_base_file(&spec, &bytes);
    archive a = {0};
    CHECK(archive_unzip(&a, bytes.data, bytes.size));

    /* A file of vendor with a pattern that is found in the zip file. */
    x3pio_document *doc = base_document(X3PIO_REVISION_AMD1);
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(x3pio_writer_set_compression(writer, &spec.compression));
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    static const unsigned char pattern[] = "VENDOR-PATTERN-0123456789";
    CHECK_OK(x3pio_add_vendor_file(writer, "http://www.vendor.com",
                                   "pattern.txt", pattern, sizeof pattern,
                                   NULL));
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
    /* The points are little-endian floats in the file. */
    unsigned char needle[4];
    x3pio_store_float(needle, (float)heights[0]);
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
        const x3pio_status status =
            x3pio_read_z(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), back);
        CHECK(status == X3PIO_ERR_FORMAT || status == X3PIO_ERR_CHECKSUM);
        /* An error ends the reading. */
        CHECK_STATUS(
            X3PIO_ERR_STATE,
            x3pio_read_z(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), back));
        x3pio_reader_free(reader);
    }
    x3pio_buffer_free(&damaged);
    archive_free(&a);
    x3pio_buffer_free(&bytes);
}

int main(void)
{
    test_disk();
#ifdef _WIN32
    test_wide_paths();
#endif
    test_memory_writer();
    test_plain_headers();
    test_vendor_files_amd1();
    test_vendor_files_2017();
    test_vendor_rules();
    test_compression();
    test_reader_states();
    test_writer_states();
    test_damage();
    return finish_tests();
}
