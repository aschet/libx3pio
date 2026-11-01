/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Real files deviate from the standard, and some are damaged. The reader
 * tolerates what it can read without doubt and refuses the rest with an error.
 * Every case here changes a valid file in one respect. */

#include "helpers.h"
#include "md5.h"

#include <stdint.h>
#include <string.h>

/* A valid file of 4 x 3 points as a set of members. */
static void base(archive *a, x3pio_storage storage, x3pio_type z_type,
                 bool missing)
{
    file_spec spec = default_spec();
    spec.storage = storage;
    spec.z_type = z_type;
    spec.missing = missing;
    create_base_archive(&spec, a);
}

static void base_binary(archive *a)
{
    base(a, X3PIO_STORAGE_BINARY, X3PIO_TYPE_DOUBLE, false);
}

static void base_text(archive *a)
{
    base(a, X3PIO_STORAGE_TEXT, X3PIO_TYPE_DOUBLE, false);
}

/* Changes main.xml and writes the checksum again. */
#define XML(a) "main.xml"
static void fixed(archive *a)
{
    archive_fix_checksum(a);
}

static void replace_first(archive *a, const char *from, const char *to)
{
    CHECK(archive_replace_first(a, "main.xml", from, to));
}

static void replace_all(archive *a, const char *from, const char *to)
{
    CHECK(archive_replace(a, "main.xml", from, to));
}

/* Opens the members and reads the heights; the status of the first step that
 * fails. The heights are in z. */
static x3pio_status try_read(archive *a, bool verify, double *z)
{
    x3pio_buffer keep;
    x3pio_buffer_init(&keep, x3pio_default_allocator());
    x3pio_status status = X3PIO_OK;
    x3pio_reader *reader = open_archive(a, &keep, verify, &status);
    if (status == X3PIO_OK)
    {
        double local[12];
        status = x3pio_read_z(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE),
                              z != NULL ? z : local);
    }
    x3pio_reader_free(reader);
    x3pio_buffer_free(&keep);
    return status;
}

static x3pio_status try_open(archive *a, bool verify)
{
    x3pio_buffer keep;
    x3pio_buffer_init(&keep, x3pio_default_allocator());
    x3pio_status status = X3PIO_OK;
    x3pio_reader *reader = open_archive(a, &keep, verify, &status);
    x3pio_reader_free(reader);
    x3pio_buffer_free(&keep);
    return status;
}

/* Opens the members and gives the reader to the caller. */
static x3pio_reader *open_for(archive *a, x3pio_buffer *keep, bool verify)
{
    x3pio_status status = X3PIO_OK;
    x3pio_reader *reader = open_archive(a, keep, verify, &status);
    CHECK_OK(status);
    return reader;
}

static void test_base_files(void)
{
    for (int st = 0; st < 2; ++st)
    {
        archive a = {0};
        base(&a, st == 0 ? X3PIO_STORAGE_BINARY : X3PIO_STORAGE_TEXT,
             X3PIO_TYPE_DOUBLE, false);
        double z[12] = {0};
        CHECK_OK(try_read(&a, true, z));
        CHECK_NEAR(z[11], 1.2e-6, 1e-18);
        archive_free(&a);
    }
}

static void test_defaults(void)
{
    archive a = {0};
    base_binary(&a);
    archive_strip_element(&a, "main.xml", "Offset");
    fixed(&a);
    x3pio_buffer keep;
    x3pio_buffer_init(&keep, x3pio_default_allocator());
    x3pio_reader *r = open_for(&a, &keep, true);
    const x3pio_transformation place =
        x3pio_document_transformation(x3pio_reader_document(r));
    CHECK(place.offset[0] == 0 && place.offset[1] == 0 && place.offset[2] == 0);
    x3pio_reader_free(r);

    /* Empty elements. */
    replace_all(&a, "<Increment>1e-06</Increment>", "<Increment/>");
    fixed(&a);
    r = open_for(&a, &keep, true);
    for (int c = 0; c < 3; ++c)
    {
        CHECK(x3pio_document_axis(x3pio_reader_document(r), (x3pio_coordinate)c)
                  .increment
              == 1.0);
    }
    x3pio_reader_free(r);
    archive_free(&a);

    base_binary(&a);
    replace_all(&a, "<Offset>0</Offset>", "<Offset></Offset>");
    fixed(&a);
    CHECK_OK(try_open(&a, true));
    archive_free(&a);
    x3pio_buffer_free(&keep);
}

static void test_data_type(void)
{
    archive a = {0};
    /* The data type of an incremental axis may be left out ... */
    base_binary(&a);
    replace_all(&a, "<AxisType>I</AxisType>\n        <DataType>D</DataType>",
                "<AxisType>I</AxisType>");
    fixed(&a);
    CHECK_OK(try_open(&a, true));
    archive_free(&a);
    /* ... but not that of an absolute axis. */
    base_binary(&a);
    replace_all(&a, "<AxisType>A</AxisType>\n        <DataType>D</DataType>",
                "<AxisType>A</AxisType>");
    fixed(&a);
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&a, true));
    archive_free(&a);
    /* An incremental axis may carry any data type. */
    base_binary(&a);
    replace_first(&a, "<AxisType>I</AxisType>\n        <DataType>D",
                  "<AxisType>I</AxisType>\n        <DataType>L");
    fixed(&a);
    CHECK_OK(try_open(&a, true));
    archive_free(&a);
}

static void test_root_names(void)
{
    archive a = {0};
    base_binary(&a);
    replace_all(&a, "p:ISO5436_2", "ISO5436_2");
    replace_first(&a, " xmlns:p=\"http://www.opengps.eu/2008/ISO5436_2\"", "");
    fixed(&a);
    CHECK_OK(try_open(&a, true));
    archive_free(&a);

    base_binary(&a);
    replace_all(&a, "p:ISO5436_2", "x:ISO5436_2");
    replace_all(&a, "xmlns:p=", "xmlns:x=");
    fixed(&a);
    CHECK_OK(try_open(&a, true));
    archive_free(&a);

    base_binary(&a);
    replace_all(&a, "p:ISO5436_2", "ISO5436_2");
    replace_all(&a, "xmlns:p=", "xmlns=");
    fixed(&a);
    CHECK_OK(try_open(&a, true));
    archive_free(&a);

    base_binary(&a);
    replace_all(&a, "p:ISO5436_2", "SomethingElse");
    fixed(&a);
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&a, true));
    archive_free(&a);
}

static void test_unknown_and_order(void)
{
    archive a = {0};
    base_binary(&a);
    replace_first(&a, "<Record1>", "<Record1><Unknown>1</Unknown>");
    replace_first(&a, "<Axes>", "<Axes unit=\"m\"><Extra/>");
    replace_first(&a, "<Record3>", "<!-- note --><Record3><Mask>1 0 1</Mask>");
    replace_first(&a, "</Record4>", "</Record4><SomethingNew>1</SomethingNew>");
    fixed(&a);
    CHECK_OK(try_read(&a, true, NULL));
    archive_free(&a);

    /* Record4 before Record1, in a file of text, where the axes are needed to
     * read the data. */
    base_text(&a);
    const char *text = archive_text(&a, "main.xml");
    const char *start = strstr(text, "<Record4>");
    const char *end = strstr(text, "</Record4>") + strlen("</Record4>");
    char record4[256];
    snprintf(record4, sizeof record4, "%.*s", (int)(end - start), start);
    CHECK(archive_cut(&a, "main.xml", "<Record4>", "</Record4>"));
    char head[300];
    snprintf(head, sizeof head, "%s<Record1>", record4);
    replace_first(&a, "<Record1>", head);
    /* Record3 before Record1 as well. */
    fixed(&a);
    CHECK_OK(try_read(&a, true, NULL));
    archive_free(&a);

    /* The data before the axes: the Datum elements are checked afterwards. */
    base_text(&a);
    text = archive_text(&a, "main.xml");
    start = strstr(text, "<Record3>");
    end = strstr(text, "</Record3>") + strlen("</Record3>");
    x3pio_buffer record3;
    x3pio_buffer_init(&record3, x3pio_default_allocator());
    x3pio_buffer_append(&record3, start, (size_t)(end - start));
    x3pio_buffer_append_char(&record3, '\0');
    CHECK(archive_cut(&a, "main.xml", "<Record3>", "</Record3>"));
    char *moved = malloc(record3.size + 20);
    snprintf(moved, record3.size + 20, "%s<Record1>",
             (const char *)record3.data);
    replace_first(&a, "<Record1>", moved);
    fixed(&a);
    double z[12] = {0};
    CHECK_OK(try_read(&a, true, z));
    CHECK_NEAR(z[11], 1.2e-6, 1e-18);
    free(moved);
    x3pio_buffer_free(&record3);
    archive_free(&a);
}

static void test_byte_order_mark(void)
{
    archive a = {0};
    base_binary(&a);
    x3pio_buffer with_mark;
    x3pio_buffer_init(&with_mark, x3pio_default_allocator());
    x3pio_buffer_append_text(&with_mark, "\xEF\xBB\xBF");
    member *main = archive_find(&a, "main.xml");
    x3pio_buffer_append(&with_mark, main->data.data, main->data.size);
    archive_put(&a, "main.xml", with_mark.data, with_mark.size);
    x3pio_buffer_free(&with_mark);
    fixed(&a); /* the checksum is over the bytes with the mark */
    x3pio_buffer keep;
    x3pio_buffer_init(&keep, x3pio_default_allocator());
    x3pio_reader *r = open_for(&a, &keep, true);
    CHECK_STRING(x3pio_document_creator(x3pio_reader_document(r)),
                 "Jane Doe, Metrology <Lab>");
    CHECK_STRING(x3pio_document_comment(x3pio_reader_document(r)),
                 "A <test> & more");
    x3pio_reader_free(r);
    x3pio_buffer_free(&keep);
    archive_free(&a);

    /* Windows line ends. */
    base_binary(&a);
    replace_all(&a, "\n", "\r\n");
    fixed(&a);
    CHECK_OK(try_open(&a, true));
    archive_free(&a);

    /* Another declared encoding that Expat knows. */
    base_binary(&a);
    replace_first(&a, "encoding=\"UTF-8\"", "encoding=\"ISO-8859-1\"");
    fixed(&a);
    CHECK_OK(try_open(&a, true));
    archive_free(&a);
}

static void test_metadata(void)
{
    archive a = {0};
    base_binary(&a);
    CHECK(archive_cut(&a, "main.xml", "<Date>", "</Date>"));
    replace_first(&a, "<Instrument>", "<Date>N/A</Date><Instrument>");
    replace_first(&a, "<Type>NonContacting</Type>", "<Type>Type</Type>");
    fixed(&a);
    x3pio_buffer keep;
    x3pio_buffer_init(&keep, x3pio_default_allocator());
    x3pio_reader *r = open_for(&a, &keep, true);
    const x3pio_document *d = x3pio_reader_document(r);
    CHECK(x3pio_document_has_metadata(d));
    CHECK(!x3pio_document_date(d, NULL));
    CHECK(!x3pio_document_calibration_date(d, NULL));
    CHECK(x3pio_document_probing_type(d) == X3PIO_PROBING_UNKNOWN);
    CHECK_STRING(x3pio_document_model(d), "Scanner 3000");
    x3pio_reader_free(r);
    archive_free(&a);

    /* A file without parts of the metadata. */
    base_binary(&a);
    CHECK(archive_cut(&a, "main.xml", "<Instrument>", "</Instrument>"));
    CHECK(archive_cut(&a, "main.xml", "<ProbingSystem>", "</ProbingSystem>"));
    archive_strip_element(&a, "main.xml", "CalibrationDate");
    archive_strip_element(&a, "main.xml", "Creator");
    fixed(&a);
    r = open_for(&a, &keep, true);
    d = x3pio_reader_document(r);
    CHECK(x3pio_document_has_metadata(d));
    CHECK_STRING(x3pio_document_manufacturer(d), "");
    CHECK_STRING(x3pio_document_creator(d), "");
    CHECK(x3pio_document_probing_type(d) == X3PIO_PROBING_UNKNOWN);
    x3pio_reader_free(r);
    archive_free(&a);

    base_binary(&a);
    CHECK(archive_cut(&a, "main.xml", "<Record2>", "</Record2>"));
    fixed(&a);
    r = open_for(&a, &keep, true);
    CHECK(!x3pio_document_has_metadata(x3pio_reader_document(r)));
    x3pio_reader_free(r);
    archive_free(&a);

    base_binary(&a);
    CHECK(archive_cut(&a, "main.xml", "<Record4>", "</Record4>"));
    fixed(&a);
    CHECK_OK(try_open(&a, true));
    archive_free(&a);
    x3pio_buffer_free(&keep);
}

/* Replaces the content of the first Datum elements. */
static void rewrite_datums(archive *a, const char *const *texts, size_t count)
{
    const char *xml = archive_text(a, "main.xml");
    x3pio_buffer out;
    x3pio_buffer_init(&out, x3pio_default_allocator());
    const char *at = xml;
    for (size_t i = 0; i < count; ++i)
    {
        const char *begin = strstr(at, "<Datum");
        const char *end = strstr(begin, "</Datum>");
        const char *self = strstr(begin, "/>");
        const char *stop =
            begin[6] == '/' ? self + 2 : end + strlen("</Datum>");
        x3pio_buffer_append(&out, at, (size_t)(begin - at));
        x3pio_buffer_append_text(&out, "<Datum>");
        x3pio_buffer_append_text(&out, texts[i]);
        x3pio_buffer_append_text(&out, "</Datum>");
        at = stop;
    }
    x3pio_buffer_append_text(&out, at);
    archive_put(a, "main.xml", out.data, out.size);
    x3pio_buffer_free(&out);
    fixed(a);
}

static void test_numbers_in_text(void)
{
    archive a = {0};
    base_text(&a);
    const char *odd[] = {"  2.e-06  ", "+3E-7", ".5e-6",
                         "1.0000000000000001e-07"};
    rewrite_datums(&a, odd, 4);
    double z[12] = {0};
    CHECK_OK(try_read(&a, true, z));
    CHECK(z[0] == 2e-6);
    CHECK(z[1] == 3e-7);
    CHECK(z[2] == 5e-7);
    CHECK(z[3] == 1.0000000000000001e-07);
    CHECK_NEAR(z[4], 5e-7, 1e-12);
    archive_free(&a);

    /* Infinity, not-a-number and nothing mean that the point is missing. */
    base_text(&a);
    const char *holes[] = {"inf", "nan", ""};
    rewrite_datums(&a, holes, 3);
    x3pio_buffer keep;
    x3pio_buffer_init(&keep, x3pio_default_allocator());
    x3pio_reader *r = open_for(&a, &keep, true);
    double metres[12];
    unsigned char flags[12];
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.z = (x3pio_field){metres, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    into.valid = (x3pio_field){flags, *x3pio_layout_packed(X3PIO_TYPE_UINT8)};
    CHECK_OK(x3pio_read_data(r, &into, 12));
    CHECK(flags[0] == 0 && flags[1] == 0 && flags[2] == 0 && flags[3] == 1);
    CHECK(isinf(metres[0]) || isnan(metres[0]));
    x3pio_reader_free(r);
    x3pio_buffer_free(&keep);
    archive_free(&a);
}

static void test_checksum_file(void)
{
    archive entries = {0};
    base_binary(&entries);
    char digest[X3PIO_MD5_HEX_SIZE];
    member *main = archive_find(&entries, "main.xml");
    x3pio_md5_of(main->data.data, main->data.size, digest);
    char upper[X3PIO_MD5_HEX_SIZE];
    for (size_t i = 0; i < sizeof upper; ++i)
    {
        upper[i] = (char)(digest[i] >= 'a' && digest[i] <= 'f' ? digest[i] - 32
                                                               : digest[i]);
    }
    char line[200];
    const char *before[] = {"", "", "", "", "", "\xEF\xBB\xBF", "  "};
    const char *after[] = {
        "", "\n", " *main.xml\n", "  main.xml\n", " *main.xml\r\n", "\n", ""};
    for (size_t i = 0; i < sizeof before / sizeof before[0]; ++i)
    {
        archive m = {0};
        base_binary(&m);
        snprintf(line, sizeof line, "%s%s%s", before[i], digest, after[i]);
        archive_put_text(&m, "md5checksum.hex", line);
        CHECK_OK(try_open(&m, true));
        snprintf(line, sizeof line, "%s%s%s", before[i], upper, after[i]);
        archive_put_text(&m, "md5checksum.hex", line);
        CHECK_OK(try_open(&m, true));
        archive_free(&m);
    }
    /* What cannot be read as a checksum is not checked. */
    const char *garbage[] = {"", "garbage", "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz",
                             "1234"};
    for (size_t i = 0; i < 4; ++i)
    {
        archive m = {0};
        base_binary(&m);
        archive_put_text(&m, "md5checksum.hex", garbage[i]);
        CHECK_OK(try_open(&m, true));
        archive_free(&m);
    }
    archive missing = {0};
    base_binary(&missing);
    archive_remove(&missing, "md5checksum.hex");
    CHECK_OK(try_open(&missing, true));
    archive_free(&missing);

    /* A checksum that does not match is an error, unless it is not asked for.
     */
    archive wrong = {0};
    base_binary(&wrong);
    archive_put_text(&wrong, "md5checksum.hex",
                     "00000000000000000000000000000000 *main.xml\n");
    CHECK_STATUS(X3PIO_ERR_CHECKSUM, try_open(&wrong, true));
    CHECK_OK(try_open(&wrong, false));
    archive_free(&wrong);

    /* A changed main.xml no longer matches. */
    archive edited = {0};
    base_binary(&edited);
    archive_put_text(&edited, "main.xml", archive_text(&edited, "main.xml"));
    x3pio_buffer more;
    x3pio_buffer_init(&more, x3pio_default_allocator());
    x3pio_buffer_append_text(&more, archive_text(&edited, "main.xml"));
    x3pio_buffer_append_char(&more, '\n');
    archive_put(&edited, "main.xml", more.data, more.size);
    x3pio_buffer_free(&more);
    CHECK_STATUS(X3PIO_ERR_CHECKSUM, try_open(&edited, true));
    archive_free(&edited);

    /* Record4 names the file. */
    archive renamed = {0};
    base_binary(&renamed);
    archive_remove(&renamed, "md5checksum.hex");
    replace_first(&renamed, "md5checksum.hex", "sum.txt");
    main = archive_find(&renamed, "main.xml");
    x3pio_md5_of(main->data.data, main->data.size, digest);
    snprintf(line, sizeof line, "%s *main.xml\n", digest);
    archive_put_text(&renamed, "sum.txt", line);
    CHECK_OK(try_open(&renamed, true));
    archive_put_text(&renamed, "sum.txt", "11111111111111111111111111111111");
    CHECK_STATUS(X3PIO_ERR_CHECKSUM, try_open(&renamed, true));
    archive_free(&renamed);

    /* A name that leaves the container is not followed. */
    archive outside = {0};
    base_binary(&outside);
    replace_first(&outside, "md5checksum.hex", "../md5checksum.hex");
    CHECK_OK(try_open(&outside, true));
    archive_free(&outside);
    archive_free(&entries);
}

static void test_point_checksum(void)
{
    archive a = {0};
    base_binary(&a);
    member *data = archive_find(&a, "bindata/data.bin");
    data->data.data[20] = (unsigned char)~data->data.data[20];

    /* Opening does not read the data, reading all of it finds the damage ... */
    CHECK_OK(try_open(&a, true));
    CHECK_STATUS(X3PIO_ERR_CHECKSUM, try_read(&a, true, NULL));
    /* ... and so does reading it in rows, at the last row. */
    x3pio_buffer keep;
    x3pio_buffer_init(&keep, x3pio_default_allocator());
    x3pio_reader *r = open_for(&a, &keep, true);
    double z[4];
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.z = (x3pio_field){z, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    CHECK_OK(x3pio_read_points(r, &into, 4));
    CHECK_OK(x3pio_read_points(r, &into, 4));
    CHECK_STATUS(X3PIO_ERR_CHECKSUM, x3pio_read_points(r, &into, 4));
    CHECK_STRING(x3pio_reader_error_message(r),
                 "The checksum of the point data does not match");
    /* An error ends the reading. */
    CHECK_STATUS(X3PIO_ERR_STATE, x3pio_read_points(r, &into, 0));
    x3pio_reader_free(r);
    /* Unless it is not asked for. */
    CHECK_OK(try_read(&a, false, NULL));
    x3pio_buffer_free(&keep);
    archive_free(&a);
}

static void test_damaged_data_files(void)
{
    archive m = {0};
    base_binary(&m);
    member *data = archive_find(&m, "bindata/data.bin");
    --data->data.size;
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&m, false));
    archive_free(&m);

    base_binary(&m);
    data = archive_find(&m, "bindata/data.bin");
    x3pio_buffer_append_char(&data->data, '\0');
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&m, false));
    archive_free(&m);

    base_binary(&m);
    archive_remove(&m, "bindata/data.bin");
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&m, true));
    archive_free(&m);

    base(&m, X3PIO_STORAGE_BINARY, X3PIO_TYPE_INT16, true);
    member *bits = archive_find(&m, "bindata/valid.bin");
    CHECK(bits != NULL);
    if (bits != NULL)
    {
        x3pio_buffer_append_char(&bits->data, (char)0xFF);
        CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&m, false));
    }
    archive_free(&m);

    base(&m, X3PIO_STORAGE_BINARY, X3PIO_TYPE_INT16, true);
    bits = archive_find(&m, "bindata/valid.bin");
    if (bits != NULL)
    {
        bits->data.data[0] = 0;
        CHECK_STATUS(X3PIO_ERR_CHECKSUM, try_open(&m, true));
        CHECK_OK(try_open(&m, false));
    }
    archive_free(&m);

    base(&m, X3PIO_STORAGE_BINARY, X3PIO_TYPE_INT16, true);
    archive_remove(&m, "bindata/valid.bin");
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&m, true));
    archive_free(&m);
}

static void test_infinite_value(void)
{
    archive a = {0};
    base_binary(&a);
    char old_digest[X3PIO_MD5_HEX_SIZE];
    member *data = archive_find(&a, "bindata/data.bin");
    x3pio_md5_of(data->data.data, data->data.size, old_digest);
    const double inf = INFINITY;
    memcpy(data->data.data + 3 * 8, &inf, 8);
    char new_digest[X3PIO_MD5_HEX_SIZE];
    x3pio_md5_of(data->data.data, data->data.size, new_digest);
    replace_first(&a, old_digest, new_digest);
    fixed(&a);
    x3pio_buffer keep;
    x3pio_buffer_init(&keep, x3pio_default_allocator());
    x3pio_reader *r = open_for(&a, &keep, true);
    double z[12];
    unsigned char flags[12];
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.z = (x3pio_field){z, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    into.valid = (x3pio_field){flags, *x3pio_layout_packed(X3PIO_TYPE_UINT8)};
    CHECK_OK(x3pio_read_data(r, &into, 12));
    CHECK(flags[3] == 0 && flags[4] == 1 && isinf(z[3]));
    x3pio_reader_free(r);
    x3pio_buffer_free(&keep);
    archive_free(&a);
}

static void test_links(void)
{
    const char *bad[] = {
        "../data.bin",        "/etc/passwd",  "http://example.com/data.bin",
        "file:///etc/passwd", "C:\\data.bin", "a/../../b"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i)
    {
        archive a = {0};
        base_binary(&a);
        replace_first(&a, "bindata/data.bin", bad[i]);
        fixed(&a);
        CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&a, true));
        archive_free(&a);
    }
    archive empty = {0};
    base_binary(&empty);
    replace_first(&empty, "<PointDataLink>bindata/data.bin</PointDataLink>",
                  "<PointDataLink></PointDataLink>");
    fixed(&empty);
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&empty, true));
    archive_free(&empty);
    /* Backslashes separate, and a leading "./" is nothing. */
    const char *good[] = {"bindata\\data.bin", "./bindata/data.bin",
                          "bindata//data.bin"};
    for (size_t i = 0; i < sizeof good / sizeof good[0]; ++i)
    {
        archive a = {0};
        base_binary(&a);
        replace_first(&a, "bindata/data.bin", good[i]);
        fixed(&a);
        CHECK_OK(try_read(&a, true, NULL));
        archive_free(&a);
    }
    /* Another name for the data file is fine. */
    archive renamed = {0};
    base_binary(&renamed);
    member *data = archive_find(&renamed, "bindata/data.bin");
    archive_put(&renamed, "data/points.bin", data->data.data, data->data.size);
    archive_remove(&renamed, "bindata/data.bin");
    replace_first(&renamed, "bindata/data.bin", "data/points.bin");
    fixed(&renamed);
    CHECK_OK(try_read(&renamed, true, NULL));
    archive_free(&renamed);
}

static void test_containers(void)
{
    archive base_a = {0};
    base_binary(&base_a);

    /* Members in a folder, as zipped by hand, with the junk of other systems.
     */
    archive folder = {0};
    for (size_t i = 0; i < base_a.count; ++i)
    {
        char name[200];
        snprintf(name, sizeof name, "scan/%s", base_a.items[i].name);
        archive_put(&folder, name, base_a.items[i].data.data,
                    base_a.items[i].data.size);
    }
    CHECK_OK(try_read(&folder, true, NULL));
    archive_put_text(&folder, "__MACOSX/scan/._main.xml", "junk");
    archive_put_text(&folder, "scan/.DS_Store", "junk");
    archive_put_text(&folder, "scan/Thumbs.db", "junk");
    archive_put_text(&folder, "scan/desktop.ini", "junk");
    archive_put_text(&folder, "scan/._main.xml", "junk");
    CHECK_OK(try_read(&folder, true, NULL));
    archive_free(&folder);

    /* A folder as an entry of its own. */
    archive with_dirs = {0};
    archive_put(&with_dirs, "bindata/", "", 0);
    for (size_t i = 0; i < base_a.count; ++i)
    {
        archive_put(&with_dirs, base_a.items[i].name, base_a.items[i].data.data,
                    base_a.items[i].data.size);
    }
    CHECK_OK(try_read(&with_dirs, true, NULL));
    archive_free(&with_dirs);

    /* Backslashes in the names, as some Windows tools write them. */
    archive windows = {0};
    for (size_t i = 0; i < base_a.count; ++i)
    {
        char name[200];
        snprintf(name, sizeof name, "%s", base_a.items[i].name);
        for (char *c = name; *c != '\0'; ++c)
        {
            if (*c == '/')
            {
                *c = '\\';
            }
        }
        archive_put(&windows, name, base_a.items[i].data.data,
                    base_a.items[i].data.size);
    }
    CHECK_OK(try_read(&windows, true, NULL));
    archive_free(&windows);

    /* A main.xml in several folders is ambiguous; a deeper one is not found. */
    archive two = {0};
    for (size_t i = 0; i < base_a.count; ++i)
    {
        char name[200];
        snprintf(name, sizeof name, "a/%s", base_a.items[i].name);
        archive_put(&two, name, base_a.items[i].data.data,
                    base_a.items[i].data.size);
        snprintf(name, sizeof name, "b/%s", base_a.items[i].name);
        archive_put(&two, name, base_a.items[i].data.data,
                    base_a.items[i].data.size);
    }
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&two, true));
    archive_free(&two);
    archive deep = {0};
    for (size_t i = 0; i < base_a.count; ++i)
    {
        char name[200];
        snprintf(name, sizeof name, "a/b/%s", base_a.items[i].name);
        archive_put(&deep, name, base_a.items[i].data.data,
                    base_a.items[i].data.size);
    }
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&deep, true));
    archive_free(&deep);

    /* No main.xml, and names that are matched exactly. */
    archive none = {0};
    base_binary(&none);
    archive_remove(&none, "main.xml");
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&none, true));
    archive_free(&none);
    archive nothing = {0};
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&nothing, true));
    archive upper = {0};
    for (size_t i = 0; i < base_a.count; ++i)
    {
        archive_put(&upper, i == 0 ? "MAIN.XML" : base_a.items[i].name,
                    base_a.items[i].data.data, base_a.items[i].data.size);
    }
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&upper, true));
    archive_free(&upper);
    archive_free(&base_a);
}

static void test_vendor_tolerance(void)
{
    archive a = {0};
    base_binary(&a);
    /* Amd 1: an id whose file is not there. */
    replace_first(
        &a, "</Record4>",
        "</Record4>\n<VendorSpecificID>http://v.org/a.bin</VendorSpecificID>");
    fixed(&a);
    x3pio_buffer keep;
    x3pio_buffer_init(&keep, x3pio_default_allocator());
    x3pio_reader *r = open_for(&a, &keep, true);
    CHECK(x3pio_reader_vendor_file_count(r) == 1);
    CHECK(x3pio_reader_vendor_file_member(r, 0) == NULL);
    x3pio_reader_free(r);
    /* ... and with the file under the slashes some tools use. */
    archive_put_text(&a, "v/org/a.bin", "data");
    r = open_for(&a, &keep, true);
    CHECK(x3pio_reader_vendor_file_count(r) == 1);
    CHECK_STRING(x3pio_reader_vendor_file_member(r, 0), "v/org/a.bin");
    unsigned char content[8];
    size_t size = 0;
    CHECK_OK(
        x3pio_read_member(r, "v/org/a.bin", content, sizeof content, &size));
    CHECK(size == 4 && memcmp(content, "data", 4) == 0);
    x3pio_reader_free(r);
    archive_free(&a);

    /* An id that is no uri for a file has no file, and is not an error. */
    base_binary(&a);
    replace_first(
        &a, "</Record4>",
        "</Record4>\n<VendorSpecificID>urn:vendor</VendorSpecificID>");
    fixed(&a);
    r = open_for(&a, &keep, true);
    CHECK(x3pio_reader_vendor_file_count(r) == 1);
    CHECK(x3pio_reader_vendor_file_member(r, 0) == NULL);
    CHECK_STRING(x3pio_reader_vendor_file_id(r, 0), "urn:vendor");
    x3pio_reader_free(r);
    archive_free(&a);

    /* Several ids turn a file into one of Amd 1 whatever the revision says. */
    base_binary(&a);
    replace_first(&a, "ISO25178-72:2017/DAM1", "ISO5436 - 2000");
    replace_first(&a, "</Record4>",
                  "</Record4><VendorSpecificID>http://a.b/c</VendorSpecificID>"
                  "<VendorSpecificID>http://a.b/d</VendorSpecificID>");
    fixed(&a);
    r = open_for(&a, &keep, true);
    CHECK(x3pio_document_revision(x3pio_reader_document(r))
          == X3PIO_REVISION_AMD1);
    x3pio_reader_free(r);
    archive_free(&a);

    /* The first revision: the one vendor owns every other file. */
    base_binary(&a);
    replace_first(&a, "ISO25178-72:2017/DAM1", "ISO5436 - 2000");
    replace_first(
        &a, "</Record4>",
        "</Record4><VendorSpecificID>http://a.b/c</VendorSpecificID>");
    fixed(&a);
    archive_put_text(&a, "photo.jpg", "jpg");
    archive_put_text(&a, "extra/settings.xml", "xml");
    r = open_for(&a, &keep, true);
    CHECK(x3pio_document_revision(x3pio_reader_document(r))
          == X3PIO_REVISION_2017);
    CHECK(x3pio_reader_vendor_file_count(r) == 2);
    for (size_t i = 0; i < 2; ++i)
    {
        CHECK_STRING(x3pio_reader_vendor_file_id(r, i), "http://a.b/c");
    }
    x3pio_reader_free(r);
    archive_free(&a);
    x3pio_buffer_free(&keep);
}

static x3pio_revision revision_of(const char *text)
{
    archive a = {0};
    base_binary(&a);
    replace_first(&a, "ISO25178-72:2017/DAM1", text);
    fixed(&a);
    x3pio_buffer keep;
    x3pio_buffer_init(&keep, x3pio_default_allocator());
    x3pio_reader *r = open_for(&a, &keep, true);
    const x3pio_revision revision =
        x3pio_document_revision(x3pio_reader_document(r));
    x3pio_reader_free(r);
    x3pio_buffer_free(&keep);
    archive_free(&a);
    return revision;
}

static void test_revision_text(void)
{
    CHECK(revision_of("ISO25178-72:2017/DAM1") == X3PIO_REVISION_AMD1);
    CHECK(revision_of("ISO25178\xE2\x80\x91"
                      "72:2017/DAM1")
          == X3PIO_REVISION_AMD1);
    CHECK(revision_of("ISO25178\xE2\x88\x92"
                      "72:2017/DAM1")
          == X3PIO_REVISION_AMD1);
    CHECK(revision_of("  ISO25178-72:2017/DAM1\n") == X3PIO_REVISION_AMD1);
    CHECK(revision_of("ISO5436 - 2000") == X3PIO_REVISION_2017);
    CHECK(revision_of("whatever") == X3PIO_REVISION_2017);
    archive a = {0};
    base_binary(&a);
    archive_strip_element(&a, "main.xml", "Revision");
    fixed(&a);
    x3pio_buffer keep;
    x3pio_buffer_init(&keep, x3pio_default_allocator());
    x3pio_reader *r = open_for(&a, &keep, true);
    CHECK(x3pio_document_revision(x3pio_reader_document(r))
          == X3PIO_REVISION_2017);
    x3pio_reader_free(r);
    x3pio_buffer_free(&keep);
    archive_free(&a);
}

static void test_rotation(void)
{
    const char *head = "<Rotation><r11>";
    const char *tail =
        "</r11><r12>0</r12><r13>0</r13><r21>0</r21><r22>1</r22>"
        "<r23>0</r23><r31>0</r31><r32>0</r32><r33>1</r33></Rotation></Axes>";
    char text[400];
    archive a = {0};
    base_binary(&a);
    snprintf(text, sizeof text, "%s%s%s", head, "1.0000000001", tail);
    replace_first(&a, "</Axes>", text);
    fixed(&a);
    x3pio_buffer keep;
    x3pio_buffer_init(&keep, x3pio_default_allocator());
    x3pio_reader *r = open_for(&a, &keep, true);
    CHECK(x3pio_document_transformation(x3pio_reader_document(r)).rotation[0]
          == 1.0);
    x3pio_reader_free(r);
    archive_free(&a);

    const char *beyond[] = {"1.5", "-2"};
    for (size_t i = 0; i < 2; ++i)
    {
        base_binary(&a);
        snprintf(text, sizeof text, "%s%s%s", head, beyond[i], tail);
        replace_first(&a, "</Axes>", text);
        fixed(&a);
        CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&a, true));
        archive_free(&a);
    }
    /* A rotation that is not complete. */
    base_binary(&a);
    replace_first(&a, "</Axes>", "<Rotation><r11>1</r11></Rotation></Axes>");
    fixed(&a);
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&a, true));
    archive_free(&a);
    /* The rotation is also found where some files put it, in Record1. */
    base_binary(&a);
    replace_first(
        &a, "</Record1>",
        "<Rotation><r11>1</r11><r12>0</r12><r13>0</r13><r21>0</r21><r22>0</r22>"
        "<r23>-1</r23><r31>0</r31><r32>1</r32><r33>0</r33></Rotation></"
        "Record1>");
    fixed(&a);
    r = open_for(&a, &keep, true);
    const x3pio_transformation place =
        x3pio_document_transformation(x3pio_reader_document(r));
    CHECK(x3pio_transformation_has_rotation(&place));
    x3pio_reader_free(r);
    x3pio_buffer_free(&keep);
    archive_free(&a);
}

static x3pio_feature feature_of(archive *a)
{
    x3pio_buffer keep;
    x3pio_buffer_init(&keep, x3pio_default_allocator());
    x3pio_reader *r = open_for(a, &keep, true);
    const x3pio_feature feature =
        x3pio_document_feature(x3pio_reader_document(r));
    x3pio_reader_free(r);
    x3pio_buffer_free(&keep);
    return feature;
}

static void test_feature_type(void)
{
    archive a = {0};
    base_binary(&a);
    replace_first(&a, "<FeatureType>SUR", "<FeatureType>PCL");
    fixed(&a);
    CHECK(feature_of(&a) == X3PIO_SURFACE);
    archive_free(&a);
    base_binary(&a);
    replace_first(&a, "<FeatureType>SUR", "<FeatureType>PRF");
    fixed(&a);
    CHECK(feature_of(&a) == X3PIO_SURFACE);
    archive_free(&a);
    base_binary(&a);
    replace_first(&a, "<FeatureType>SUR", "<FeatureType> sur ");
    fixed(&a);
    CHECK(feature_of(&a) == X3PIO_SURFACE);
    archive_free(&a);

    /* A cloud keeps its feature type whatever the file says. */
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(&doc));
    CHECK_OK(
        x3pio_document_set_feature_point_cloud(doc, 3, X3PIO_TYPE_DOUBLE, 1.0));
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    const double x[3] = {1, 2, 3};
    const double y[3] = {4, 5, 6};
    const double z[3] = {7, 8, 9};
    x3pio_const_fields from;
    x3pio_const_fields_init(&from);
    from.x = (x3pio_const_field){x, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    from.y = (x3pio_const_field){y, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    from.z = (x3pio_const_field){z, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    CHECK_OK(x3pio_write_points(writer, &from, 3));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);
    x3pio_document_free(doc);
    CHECK(archive_unzip(&a, bytes.data, bytes.size));
    replace_first(&a, "<FeatureType>PCL", "<FeatureType>SUR");
    fixed(&a);
    CHECK(feature_of(&a) == X3PIO_POINT_CLOUD);
    /* A cloud with an incremental axis is refused. */
    archive_free(&a);
    CHECK(archive_unzip(&a, bytes.data, bytes.size));
    replace_first(&a, "<AxisType>A</AxisType>", "<AxisType>I</AxisType>");
    fixed(&a);
    CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&a, true));
    archive_free(&a);
    x3pio_buffer_free(&bytes);
}

typedef void (*change)(archive *);

static void refuse_with(change fn, const char *what)
{
    for (int st = 0; st < 2; ++st)
    {
        archive a = {0};
        base(&a, st == 0 ? X3PIO_STORAGE_BINARY : X3PIO_STORAGE_TEXT,
             X3PIO_TYPE_DOUBLE, false);
        fn(&a);
        fixed(&a);
        const x3pio_status status = try_open(&a, true);
        CHECK(status == X3PIO_ERR_FORMAT);
        if (status != X3PIO_ERR_FORMAT)
        {
            fprintf(stderr, "  ^ %s (%s)\n", what, st == 0 ? "binary" : "text");
        }
        archive_free(&a);
    }
}

static void c_incremental_z(archive *a)
{
    replace_first(a, "<CZ>\n        <AxisType>A", "<CZ>\n        <AxisType>I");
}
static void c_axis_type(archive *a)
{
    replace_first(a, "<AxisType>I", "<AxisType>Q");
}
static void c_data_type(archive *a)
{
    replace_first(a, "<DataType>D", "<DataType>Q");
}
static void c_feature(archive *a)
{
    replace_first(a, "<FeatureType>SUR", "<FeatureType>XYZ");
}
static void c_zero_increment(archive *a)
{
    replace_first(a, "<Increment>1e-06", "<Increment>0");
}
static void c_word_increment(archive *a)
{
    replace_first(a, "<Increment>1e-06", "<Increment>wide");
}
static void c_inf_increment(archive *a)
{
    replace_first(a, "<Increment>1e-06", "<Increment>inf");
}
static void c_word_offset(archive *a)
{
    replace_first(a, "<Offset>0</Offset>", "<Offset>left</Offset>");
}
static void c_no_record1(archive *a)
{
    CHECK(archive_cut(a, "main.xml", "<Record1>", "</Record1>"));
}
static void c_no_record3(archive *a)
{
    CHECK(archive_cut(a, "main.xml", "<Record3>", "</Record3>"));
}
static void c_no_axes(archive *a)
{
    CHECK(archive_cut(a, "main.xml", "<Axes>", "</Axes>"));
}
static void c_no_cy(archive *a)
{
    CHECK(archive_cut(a, "main.xml", "<CY>", "</CY>"));
}
static void c_no_feature(archive *a)
{
    archive_strip_element(a, "main.xml", "FeatureType");
}
static void c_no_axis_type(archive *a)
{
    archive_strip_element(a, "main.xml", "AxisType");
}
static void c_negative_size(archive *a)
{
    replace_first(a, "<SizeX>4", "<SizeX>-4");
}
static void c_fractional_size(archive *a)
{
    replace_first(a, "<SizeX>4", "<SizeX>4.5");
}
static void c_word_size(archive *a)
{
    replace_first(a, "<SizeX>4", "<SizeX>four");
}
static void c_no_size(archive *a)
{
    archive_strip_element(a, "main.xml", "SizeY");
}
static void c_both_dimensions(archive *a)
{
    replace_first(a, "<MatrixDimension>",
                  "<ListDimension>12</ListDimension><MatrixDimension>");
}
static void c_no_dimension(archive *a)
{
    CHECK(
        archive_cut(a, "main.xml", "<MatrixDimension>", "</MatrixDimension>"));
}
static void c_not_xml(archive *a)
{
    archive_put_text(a, "main.xml", "this is not XML");
}
static void c_empty(archive *a)
{
    archive_put_text(a, "main.xml", "");
}
static void c_other_xml(archive *a)
{
    archive_put_text(a, "main.xml", "<root/>");
}
static void c_unbalanced(archive *a)
{
    replace_first(a, "</Record4>", "");
}
static void c_entity(archive *a)
{
    archive_replace_first(
        a, "main.xml", "<p:ISO5436_2",
        "<!DOCTYPE p:ISO5436_2 [<!ENTITY x \"y\">]><p:ISO5436_2");
}
static void c_undefined_entity(archive *a)
{
    replace_first(a, "<Revision>", "<Revision>&unknown;");
}

static void test_refused(void)
{
    refuse_with(c_incremental_z, "an incremental z axis");
    refuse_with(c_axis_type, "an unknown axis type");
    refuse_with(c_data_type, "an unknown data type");
    refuse_with(c_feature, "an unknown feature type");
    refuse_with(c_zero_increment, "a zero increment");
    refuse_with(c_word_increment, "a non-numeric increment");
    refuse_with(c_inf_increment, "an infinite increment");
    refuse_with(c_word_offset, "a non-numeric offset");
    refuse_with(c_no_record1, "no Record1");
    refuse_with(c_no_record3, "no Record3");
    refuse_with(c_no_axes, "no Axes");
    refuse_with(c_no_cy, "no CY");
    refuse_with(c_no_feature, "no FeatureType");
    refuse_with(c_no_axis_type, "no AxisType");
    refuse_with(c_negative_size, "a negative size");
    refuse_with(c_fractional_size, "a fractional size");
    refuse_with(c_word_size, "a size that is no number");
    refuse_with(c_no_size, "a missing size");
    refuse_with(c_both_dimensions, "both kinds of dimension");
    refuse_with(c_no_dimension, "no dimension");
    refuse_with(c_not_xml, "text that is not XML");
    refuse_with(c_empty, "an empty document");
    refuse_with(c_other_xml, "an XML document of something else");
    refuse_with(c_unbalanced, "unbalanced tags");
    refuse_with(c_entity, "an entity declaration");
    refuse_with(c_undefined_entity, "an entity that is not defined");
}

static void t_one_too_few(archive *a)
{
    const char *text = archive_text(a, "main.xml");
    const char *at = NULL;
    for (const char *p = text; (p = strstr(p, "<Datum>")) != NULL; ++p)
    {
        at = p;
    }
    CHECK(at != NULL);
    if (at == NULL)
    {
        return;
    }
    const char *end = strstr(at, "</Datum>") + strlen("</Datum>");
    x3pio_buffer out;
    x3pio_buffer_init(&out, x3pio_default_allocator());
    x3pio_buffer_append(&out, text, (size_t)(at - text));
    x3pio_buffer_append_text(&out, end);
    archive_put(a, "main.xml", out.data, out.size);
    x3pio_buffer_free(&out);
}
static void t_one_too_many(archive *a)
{
    replace_first(a, "<Datum>", "<Datum>1</Datum><Datum>");
}
static void t_two_numbers(archive *a)
{
    replace_first(a, "<Datum>", "<Datum>1;2;");
}
static void t_word(archive *a)
{
    replace_first(a, "<Datum>", "<Datum>abc");
}
static void t_comma(archive *a)
{
    replace_first(a, "<Datum>", "<Datum>1,5#");
}
static void t_no_data(archive *a)
{
    CHECK(archive_cut(a, "main.xml", "<DataList>", "</DataList>"));
}
static void t_both(archive *a)
{
    replace_first(a, "<DataList>",
                  "<DataLink><PointDataLink>bindata/data.bin</PointDataLink></"
                  "DataLink><DataList>");
}

static void test_text_data(void)
{
    typedef struct item
    {
        change fn;
        const char *what;
    } item;
    const item items[] = {{t_one_too_few, "one Datum too few"},
                          {t_one_too_many, "one Datum too many"},
                          {t_two_numbers, "two numbers for one axis"},
                          {t_word, "a Datum that is no number"},
                          {t_comma, "a Datum with a decimal comma"},
                          {t_no_data, "no DataList and no DataLink"},
                          {t_both, "both a DataList and a DataLink"}};
    for (size_t i = 0; i < sizeof items / sizeof items[0]; ++i)
    {
        archive a = {0};
        base_text(&a);
        items[i].fn(&a);
        fixed(&a);
        const x3pio_status status = try_open(&a, true);
        CHECK(status == X3PIO_ERR_FORMAT);
        if (status != X3PIO_ERR_FORMAT)
        {
            fprintf(stderr, "  ^ %s\n", items[i].what);
        }
        archive_free(&a);
    }
    /* Integer axes hold whole numbers within the range of the type. */
    const char *bad[] = {"1.5", "40000", "-40000", "1e10"};
    for (size_t i = 0; i < 4; ++i)
    {
        archive a = {0};
        base(&a, X3PIO_STORAGE_TEXT, X3PIO_TYPE_INT16, false);
        const char *texts[1] = {bad[i]};
        rewrite_datums(&a, texts, 1);
        CHECK_STATUS(X3PIO_ERR_FORMAT, try_open(&a, true));
        archive_free(&a);
    }
}

/* A document type declaration without entities is nothing to worry about. */
static void test_doctype(void)
{
    archive a = {0};
    base_binary(&a);
    replace_first(&a, "<p:ISO5436_2",
                  "<!DOCTYPE p:ISO5436_2 [<!-- nothing -->]><p:ISO5436_2");
    fixed(&a);
    CHECK_OK(try_read(&a, true, NULL));
    archive_free(&a);
}

/* Zip files that are damaged are refused, not crashed on. */
static void test_damaged_zip(void)
{
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    file_spec spec = default_spec();
    create_base_file(&spec, &bytes);
    unsigned long long state = 0x9E3779B97F4A7C15ULL;
    for (size_t length = 0; length < bytes.size; length += 7)
    {
        x3pio_reader *reader = NULL;
        CHECK_OK(x3pio_reader_create(&reader));
        const x3pio_status status =
            x3pio_open_memory(reader, bytes.data, length);
        CHECK(status == X3PIO_ERR_FORMAT || status == X3PIO_OK);
        x3pio_reader_free(reader);
    }
    for (int round = 0; round < 400; ++round)
    {
        x3pio_buffer damaged;
        x3pio_buffer_init(&damaged, x3pio_default_allocator());
        x3pio_buffer_append(&damaged, bytes.data, bytes.size);
        for (int flips = 0; flips < 3; ++flips)
        {
            state = state * 6364136223846793005ULL + 1442695040888963407ULL;
            damaged.data[(size_t)(state >> 33U) % damaged.size] ^=
                (unsigned char)(1U << ((state >> 20U) % 8U));
        }
        x3pio_reader *reader = NULL;
        CHECK_OK(x3pio_reader_create(&reader));
        const x3pio_status status =
            x3pio_open_memory(reader, damaged.data, damaged.size);
        if (status == X3PIO_OK)
        {
            double z[12];
            (void)x3pio_read_z(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE),
                               z);
        }
        x3pio_reader_free(reader);
        x3pio_buffer_free(&damaged);
    }
    /* A zip file that is not an x3p file. */
    archive a = {0};
    archive_put_text(&a, "readme.txt", "hello");
    x3pio_buffer other;
    x3pio_buffer_init(&other, x3pio_default_allocator());
    CHECK(archive_zip(&a, &other));
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_STATUS(X3PIO_ERR_FORMAT,
                 x3pio_open_memory(reader, other.data, other.size));
    x3pio_reader_free(reader);
    x3pio_buffer_free(&other);
    archive_free(&a);
    x3pio_buffer_free(&bytes);
}

int main(void)
{
    test_base_files();
    test_defaults();
    test_data_type();
    test_root_names();
    test_unknown_and_order();
    test_byte_order_mark();
    test_metadata();
    test_numbers_in_text();
    test_checksum_file();
    test_point_checksum();
    test_damaged_data_files();
    test_infinite_value();
    test_links();
    test_containers();
    test_vendor_tolerance();
    test_revision_text();
    test_rotation();
    test_feature_type();
    test_refused();
    test_text_data();
    test_doctype();
    test_damaged_zip();
    return finish_tests();
}
