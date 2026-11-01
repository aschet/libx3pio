/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

#include "helpers.h"
#include "md5.h"
#include "minizip.h"

#include <libxml/parser.h>
#include <libxml/xmlschemas.h>
#include <string.h>

const char *test_path(const char *relative)
{
    static char path[4][1024];
    static size_t next = 0;
    char *out = path[next++ % 4];
    snprintf(out, 1024, "%s/%s", X3PIO_TEST_DATA_DIR, relative);
    return out;
}

unsigned char *read_test_file(const char *relative, size_t *size)
{
    FILE *file = fopen(test_path(relative), "rb");
    if (file == NULL)
    {
        return NULL;
    }
    unsigned char *data = NULL;
    size_t used = 0;
    size_t capacity = 0;
    unsigned char block[8192];
    size_t got = 0;
    do
    {
        got = fread(block, 1, sizeof block, file);
        if (used + got + 1 > capacity)
        {
            capacity = (used + got + 1) * 2;
            // The file is a file of the test data.
            // NOLINTNEXTLINE(clang-analyzer-optin.taint.TaintedAlloc)
            unsigned char *grown = realloc(data, capacity);
            if (grown == NULL)
            {
                free(data);
                fclose(file);
                return NULL;
            }
            data = grown;
        }
        memcpy(data + used, block, got);
        used += got;
    } while (got == sizeof block);
    data[used] = '\0';
    *size = used;
    fclose(file);
    return data;
}

/* ---- archives --------------------------------------------------------------
 */

void archive_free(archive *a)
{
    for (size_t i = 0; i < a->count; ++i)
    {
        x3pio_buffer_free(&a->items[i].data);
    }
    free(a->items);
    *a = (archive){0};
}

static member *archive_add(archive *a, const char *name)
{
    if (a->count == a->capacity)
    {
        const size_t capacity = a->capacity != 0 ? a->capacity * 2 : 8;
        member *grown = realloc(a->items, capacity * sizeof *grown);
        if (grown == NULL)
        {
            return NULL;
        }
        a->items = grown;
        a->capacity = capacity;
    }
    member *m = &a->items[a->count++];
    memset(m, 0, sizeof *m);
    snprintf(m->name, sizeof m->name, "%s", name);
    x3pio_buffer_init(&m->data, x3pio_default_allocator());
    m->method = MZ_COMPRESS_METHOD_DEFLATE;
    return m;
}

/* The separator of the names in a zip file is the one that the zip library
 * writes, which depends on its version and on the platform: both are the same
 * name here. */
static bool same_name(const char *a, const char *b)
{
    for (; *a != '\0' && *b != '\0'; ++a, ++b)
    {
        const char x = *a == '\\' ? '/' : *a;
        const char y = *b == '\\' ? '/' : *b;
        if (x != y)
        {
            return false;
        }
    }
    return *a == *b;
}

member *archive_find(archive *a, const char *name)
{
    for (size_t i = 0; i < a->count; ++i)
    {
        if (same_name(a->items[i].name, name))
        {
            return &a->items[i];
        }
    }
    return NULL;
}

void archive_put(archive *a, const char *name, const void *data, size_t size)
{
    member *m = archive_find(a, name);
    if (m == NULL)
    {
        m = archive_add(a, name);
    }
    m->data.size = 0;
    x3pio_buffer_append(&m->data, data, size);
}

void archive_put_text(archive *a, const char *name, const char *text)
{
    archive_put(a, name, text, strlen(text));
}

void archive_remove(archive *a, const char *name)
{
    for (size_t i = 0; i < a->count; ++i)
    {
        if (strcmp(a->items[i].name, name) == 0)
        {
            x3pio_buffer_free(&a->items[i].data);
            memmove(&a->items[i], &a->items[i + 1],
                    (a->count - i - 1) * sizeof a->items[i]);
            --a->count;
            return;
        }
    }
}

const char *archive_text(archive *a, const char *name)
{
    member *m = archive_find(a, name);
    if (m == NULL)
    {
        return "";
    }
    x3pio_buffer_reserve(&m->data, 1);
    m->data.data[m->data.size] = '\0';
    return (const char *)m->data.data;
}

bool archive_replace(archive *a, const char *name, const char *from,
                     const char *to)
{
    member *m = archive_find(a, name);
    if (m == NULL)
    {
        return false;
    }
    const char *text = archive_text(a, name);
    const size_t from_size = strlen(from);
    x3pio_buffer out;
    x3pio_buffer_init(&out, x3pio_default_allocator());
    bool found = false;
    const char *at = text;
    for (;;)
    {
        const char *hit = strstr(at, from);
        if (hit == NULL || from_size == 0)
        {
            x3pio_buffer_append_text(&out, at);
            break;
        }
        found = true;
        x3pio_buffer_append(&out, at, (size_t)(hit - at));
        x3pio_buffer_append_text(&out, to);
        at = hit + from_size;
    }
    archive_put(a, name, out.data, out.size);
    x3pio_buffer_free(&out);
    return found;
}

bool archive_replace_first(archive *a, const char *name, const char *from,
                           const char *to)
{
    const char *text = archive_text(a, name);
    const char *hit = strstr(text, from);
    if (hit == NULL)
    {
        return false;
    }
    x3pio_buffer out;
    x3pio_buffer_init(&out, x3pio_default_allocator());
    x3pio_buffer_append(&out, text, (size_t)(hit - text));
    x3pio_buffer_append_text(&out, to);
    x3pio_buffer_append_text(&out, hit + strlen(from));
    archive_put(a, name, out.data, out.size);
    x3pio_buffer_free(&out);
    return true;
}

bool archive_cut(archive *a, const char *name, const char *begin,
                 const char *end)
{
    const char *text = archive_text(a, name);
    const char *from = strstr(text, begin);
    if (from == NULL)
    {
        return false;
    }
    const char *to = strstr(from, end);
    if (to == NULL)
    {
        return false;
    }
    to += strlen(end);
    x3pio_buffer out;
    x3pio_buffer_init(&out, x3pio_default_allocator());
    x3pio_buffer_append(&out, text, (size_t)(from - text));
    x3pio_buffer_append_text(&out, to);
    archive_put(a, name, out.data, out.size);
    x3pio_buffer_free(&out);
    return true;
}

void archive_strip_element(archive *a, const char *name, const char *tag)
{
    char open[80];
    char close[80];
    char empty[80];
    snprintf(open, sizeof open, "<%s>", tag);
    snprintf(close, sizeof close, "</%s>", tag);
    snprintf(empty, sizeof empty, "<%s/>", tag);
    while (archive_cut(a, name, open, close))
    {
    }
    while (archive_replace_first(a, name, empty, ""))
    {
    }
}

void archive_fix_checksum(archive *a)
{
    member *main = archive_find(a, "main.xml");
    char digest[X3PIO_MD5_HEX_SIZE];
    x3pio_md5_of(main->data.data, main->data.size, digest);
    char line[64];
    snprintf(line, sizeof line, "%s *main.xml\n", digest);
    archive_put_text(a, "md5checksum.hex", line);
}

bool archive_unzip(archive *a, const void *zip, size_t size)
{
    /* minizip-ng takes a pointer to memory it may write to; the zip file is
     * copied for it. */
    unsigned char *copy = malloc(size != 0 ? size : 1);
    memcpy(copy, zip, size);
    void *stream = mz_stream_mem_create();
    mz_stream_mem_set_buffer(stream, copy, (int32_t)size);
    void *reader = mz_zip_reader_create();
    bool ok = mz_stream_open(stream, NULL, MZ_OPEN_MODE_READ) == MZ_OK
              && mz_zip_reader_open(reader, stream) == MZ_OK;
    if (ok)
    {
        for (int32_t code = mz_zip_reader_goto_first_entry(reader);
             code == MZ_OK; code = mz_zip_reader_goto_next_entry(reader))
        {
            mz_zip_file *info = NULL;
            if (mz_zip_reader_entry_get_info(reader, &info) != MZ_OK
                || mz_zip_reader_entry_is_dir(reader) == MZ_OK)
            {
                continue;
            }
            member *m = archive_add(a, info->filename);
            m->method = info->compression_method;
            if (mz_zip_reader_entry_open(reader) != MZ_OK)
            {
                ok = false;
                break;
            }
            unsigned char block[8192];
            for (;;)
            {
                const int32_t got =
                    mz_zip_reader_entry_read(reader, block, sizeof block);
                if (got < 0)
                {
                    ok = false;
                    break;
                }
                if (got == 0)
                {
                    break;
                }
                x3pio_buffer_append(&m->data, block, (size_t)got);
            }
            mz_zip_reader_entry_close(reader);
        }
    }
    mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);
    mz_stream_close(stream);
    mz_stream_mem_delete(&stream);
    free(copy);
    return ok;
}

bool archive_zip(const archive *a, x3pio_buffer *out)
{
    void *stream = mz_stream_mem_create();
    mz_stream_mem_set_grow_size(stream, 65536);
    void *writer = mz_zip_writer_create();
    bool ok = mz_stream_open(stream, NULL, MZ_OPEN_MODE_CREATE) == MZ_OK
              && mz_zip_writer_open(writer, stream, 0) == MZ_OK;
    mz_zip_writer_set_compress_method(writer, MZ_COMPRESS_METHOD_DEFLATE);
    for (size_t i = 0; ok && i < a->count; ++i)
    {
        mz_zip_file info;
        memset(&info, 0, sizeof info);
        info.version_madeby = MZ_VERSION_MADEBY;
        info.compression_method = (uint16_t)a->items[i].method;
        info.flag = MZ_ZIP_FLAG_UTF8;
        info.zip64 = MZ_ZIP64_AUTO;
        info.modified_date = 315662400;
        info.filename = a->items[i].name;
        ok = mz_zip_writer_entry_open(writer, &info) == MZ_OK;
        if (ok && a->items[i].data.size > 0)
        {
            ok = mz_zip_writer_entry_write(writer, a->items[i].data.data,
                                           (int32_t)a->items[i].data.size)
                 == (int32_t)a->items[i].data.size;
        }
        ok = ok && mz_zip_writer_entry_close(writer) == MZ_OK;
    }
    ok = ok && mz_zip_writer_close(writer) == MZ_OK;
    if (ok)
    {
        const void *buffer = NULL;
        int32_t size = 0;
        mz_stream_mem_get_buffer(stream, &buffer);
        mz_stream_mem_get_buffer_length(stream, &size);
        out->size = 0;
        ok = x3pio_buffer_append(out, buffer, (size_t)size);
    }
    mz_zip_writer_delete(&writer);
    mz_stream_close(stream);
    mz_stream_mem_delete(&stream);
    return ok;
}

/* ---- the schemas
 * ------------------------------------------------------------- */

static void collect_error(void *context, const char *format, ...)
{
    (void)context;
    (void)format;
}

bool xsd_validate(const char *schema, const void *xml, size_t size, char *error,
                  size_t capacity)
{
    xmlInitParser();
    xmlSetGenericErrorFunc(NULL, collect_error);
    char path[1024];
    snprintf(path, sizeof path, "%s/xsd/%s", X3PIO_TEST_DATA_DIR, schema);
    xmlSchemaParserCtxtPtr parser = xmlSchemaNewParserCtxt(path);
    xmlSchemaPtr compiled = xmlSchemaParse(parser);
    xmlSchemaFreeParserCtxt(parser);
    if (compiled == NULL)
    {
        snprintf(error, capacity, "the schema %s cannot be loaded", schema);
        return false;
    }
    xmlDocPtr doc = xmlReadMemory(xml, (int)size, "main.xml", NULL, 0);
    bool valid = false;
    if (doc == NULL)
    {
        snprintf(error, capacity, "main.xml is not well-formed");
    }
    else
    {
        xmlSchemaValidCtxtPtr context = xmlSchemaNewValidCtxt(compiled);
        const int result = xmlSchemaValidateDoc(context, doc);
        valid = result == 0;
        if (!valid)
        {
            snprintf(error, capacity, "main.xml is not valid against %s (%d)",
                     schema, result);
        }
        xmlSchemaFreeValidCtxt(context);
        xmlFreeDoc(doc);
    }
    xmlSchemaFree(compiled);
    return valid;
}

/* ---- builders
 * ----------------------------------------------------------------- */

file_spec default_spec(void)
{
    file_spec spec = {0};
    spec.revision = X3PIO_REVISION_AMD1;
    spec.storage = X3PIO_STORAGE_BINARY;
    spec.z_type = X3PIO_TYPE_DOUBLE;
    spec.metadata = true;
    spec.compression = x3pio_compression_default();
    return spec;
}

x3pio_status create_in_buffer(x3pio_writer *writer, const x3pio_document *doc,
                              x3pio_buffer *out)
{
    return x3pio_create_memory_into(writer, doc, out);
}

bool read_whole_file(const char *path, x3pio_buffer *out)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        return false;
    }
    out->size = 0;
    unsigned char block[8192];
    size_t got = 0;
    do
    {
        got = fread(block, 1, sizeof block, file);
        x3pio_buffer_append(out, block, got);
    } while (got == sizeof block);
    fclose(file);
    return true;
}

void create_base_file(const file_spec *spec, x3pio_buffer *out)
{
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(&doc));
    CHECK_OK(x3pio_document_set_revision(doc, spec->revision));
    const double increment =
        spec->z_type == X3PIO_TYPE_DOUBLE || spec->z_type == X3PIO_TYPE_FLOAT
            ? 1.0
            : 1e-9;
    CHECK_OK(x3pio_document_set_feature_surface(doc, 4, 3, 1e-6, 1e-6,
                                                spec->z_type, increment, 1));
    if (spec->metadata)
    {
        x3pio_timestamp date = {2026, 3, 4, 12, 30, 0, 0, true, 60};
        CHECK_OK(x3pio_document_set_date(doc, &date));
        CHECK_OK(x3pio_document_set_creator(doc, "Jane Doe, Metrology <Lab>"));
        CHECK_OK(x3pio_document_set_manufacturer(doc, "Acme"));
        CHECK_OK(x3pio_document_set_model(doc, "Scanner 3000"));
        CHECK_OK(x3pio_document_set_serial(doc, "42"));
        CHECK_OK(x3pio_document_set_version(doc, "1.0"));
        CHECK_OK(
            x3pio_document_set_probing_type(doc, X3PIO_PROBING_NON_CONTACTING));
        CHECK_OK(x3pio_document_set_probing_identification(doc, "confocal"));
        CHECK_OK(x3pio_document_set_comment(doc, "A <test> & more"));
    }
    double z[12];
    for (size_t i = 0; i < 12; ++i)
    {
        z[i] = spec->missing && i == 5 ? NAN : 1e-7 * (double)(i + 1);
    }
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(x3pio_writer_set_storage(writer, spec->storage));
    CHECK_OK(x3pio_writer_set_compression(writer, &spec->compression));
    CHECK_OK(create_in_buffer(writer, doc, out));
    CHECK_OK(x3pio_write_z(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);
    x3pio_document_free(doc);
}

void create_base_archive(const file_spec *spec, archive *a)
{
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    create_base_file(spec, &bytes);
    CHECK(archive_unzip(a, bytes.data, bytes.size));
    x3pio_buffer_free(&bytes);
}

x3pio_reader *open_archive(const archive *a, x3pio_buffer *keep, bool verify,
                           x3pio_status *status)
{
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_reader_set_verify(reader, verify));
    CHECK(archive_zip(a, keep));
    *status = x3pio_open_memory(reader, keep->data, keep->size);
    return reader;
}

double *read_heights_of(x3pio_reader *reader)
{
    const size_t count =
        x3pio_document_point_count(x3pio_reader_document(reader));
    double *z = malloc(count * sizeof *z);
    CHECK_OK(x3pio_read_z(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
    return z;
}
