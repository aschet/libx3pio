/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The sample files read as the reference implementation, the Python library
 * x3pio, reads them: expected.txt holds what it reads. Every file is also
 * written again in both revisions and both storages, valid against the schemas,
 * with the same points. */

#include "helpers.h"

#include <x3pio/file.h>

#include <stdint.h>
#include <string.h>

#ifndef _WIN32
#include <dirent.h>
#endif

typedef struct expectation
{
    char path[1024];
    char feature[1024];
    size_t layers;
    char shape[1024];
    size_t valid;
    double sum_x;
    double sum_y;
    double sum_z;
    double z_first;
    double z_last;
} expectation;

/* Splits a line at the tabs. */
static size_t split_tabs(char *line, char **fields, size_t capacity)
{
    size_t count = 0;
    char *at = line;
    while (count < capacity)
    {
        fields[count++] = at;
        char *tab = strchr(at, '\t');
        if (tab == NULL)
        {
            break;
        }
        *tab = '\0';
        at = tab + 1;
    }
    return count;
}

static size_t load_expectations(const char *file, expectation **out)
{
    FILE *in = fopen(file, "rb");
    if (in == NULL)
    {
        return 0;
    }
    size_t count = 0;
    size_t capacity = 0;
    expectation *list = NULL;
    char line[1024];
    while (fgets(line, sizeof line, in) != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';
        char *fields[10];
        if (split_tabs(line, fields, 10) != 10)
        {
            continue;
        }
        if (count == capacity)
        {
            capacity = capacity != 0 ? capacity * 2 : 64;
            expectation *grown = realloc(list, capacity * sizeof *list);
            if (grown == NULL)
            {
                free(list);
                fclose(in);
                return 0;
            }
            list = grown;
        }
        expectation *e = &list[count++];
        snprintf(e->path, sizeof e->path, "%s", fields[0]);
        snprintf(e->feature, sizeof e->feature, "%s", fields[1]);
        e->layers = (size_t)strtoull(fields[2], NULL, 10);
        snprintf(e->shape, sizeof e->shape, "%s", fields[3]);
        e->valid = (size_t)strtoull(fields[4], NULL, 10);
        e->sum_x = strtod(fields[5], NULL);
        e->sum_y = strtod(fields[6], NULL);
        e->sum_z = strtod(fields[7], NULL);
        e->z_first = strtod(fields[8], NULL);
        e->z_last = strtod(fields[9], NULL);
    }
    fclose(in);
    *out = list;
    return count;
}

static const char *feature_name(x3pio_feature feature)
{
    switch (feature)
    {
    case X3PIO_PROFILE:
        return "profile";
    case X3PIO_SURFACE:
        return "surface";
    case X3PIO_POINT_CLOUD:
        break;
    }
    return "pointcloud";
}

/* The shape of the points with the dimensions of size one dropped: "4x4". */
static void shape_of(const x3pio_document *doc, char *text, size_t capacity)
{
    const x3pio_dimensions dims = x3pio_document_dimensions(doc);
    size_t sizes[3] = {dims.size_z, dims.size_y, dims.size_x};
    /* The reference states the first layer. */
    char out[64] = "";
    size_t used = 0;
    for (size_t i = 1; i < 3; ++i)
    {
        if (sizes[i] != 1)
        {
            used += (size_t)snprintf(out + used, sizeof out - used, "%s%zu",
                                     used > 0 ? "x" : "", sizes[i]);
        }
    }
    if (used == 0)
    {
        snprintf(out, sizeof out, "1");
    }
    snprintf(text, capacity, "%s", out);
}

typedef struct points
{
    size_t count;
    double *x;
    double *y;
    double *z;
} points;

static void points_free(points *p)
{
    free(p->x);
    free(p->y);
    free(p->z);
    *p = (points){0};
}

/* The coordinates of all points in the global system. */
static bool read_global(x3pio_reader *reader, points *p)
{
    const x3pio_document *doc = x3pio_reader_document(reader);
    p->count = x3pio_document_point_count(doc);
    p->x = malloc(p->count * sizeof *p->x);
    p->y = malloc(p->count * sizeof *p->y);
    p->z = malloc(p->count * sizeof *p->z);
    x3pio_fields into;
    x3pio_fields_init(&into);
    const x3pio_layout *packed = x3pio_layout_packed(X3PIO_TYPE_DOUBLE);
    into.x = (x3pio_field){p->x, *packed};
    into.y = (x3pio_field){p->y, *packed};
    into.z = (x3pio_field){p->z, *packed};
    CHECK_OK(x3pio_read_points(reader, &into, x3pio_document_point_count(doc)));
    const x3pio_transformation place = x3pio_document_transformation(doc);
    for (size_t i = 0; i < p->count; ++i)
    {
        const double view[3] = {p->x[i], p->y[i], p->z[i]};
        double global[3];
        x3pio_transformation_to_global(&place, view, global);
        p->x[i] = global[0];
        p->y[i] = global[1];
        p->z[i] = global[2];
    }
    return true;
}

static bool point_valid(const points *p, size_t i)
{
    return isfinite(p->x[i]) && isfinite(p->y[i]) && isfinite(p->z[i]);
}

static const char *schema_of(x3pio_revision revision)
{
    return revision == X3PIO_REVISION_AMD1 ? "iso25178-72-fdam1.xsd"
                                           : "iso25178-72-2017.xsd";
}

static bool near_relative(double a, double b, double relative)
{
    return fabs(a - b) <= relative * fmax(1e-12, fmax(fabs(a), fabs(b)))
           || fabs(a - b) <= 1e-18;
}

/* Writes the file again with the stored values and the mask, and returns the
 * bytes. */
static bool rewrite(x3pio_reader *reader, x3pio_revision revision,
                    x3pio_storage storage, x3pio_buffer *out)
{
    const x3pio_document *doc = x3pio_reader_document(reader);
    x3pio_document *copy = NULL;
    CHECK_OK(x3pio_document_create(&copy));
    CHECK_OK(x3pio_document_copy(copy, doc));
    CHECK_OK(x3pio_document_set_revision(copy, revision));
    const size_t count = x3pio_document_point_count(doc);
    const size_t total = x3pio_document_point_count(doc);

    unsigned char *stored[3] = {NULL, NULL, NULL};
    unsigned char *mask = malloc(count);
    x3pio_fields into;
    x3pio_fields_init(&into);
    x3pio_field *slots[3] = {&into.x, &into.y, &into.z};
    for (size_t k = 0; k < 3; ++k)
    {
        const x3pio_axis axis = x3pio_document_axis(doc, (x3pio_coordinate)k);
        if (axis.type == X3PIO_AXIS_ABSOLUTE)
        {
            stored[k] = malloc(count * 8);
            *slots[k] =
                (x3pio_field){stored[k], *x3pio_layout_packed(axis.storage)};
        }
    }
    into.valid = (x3pio_field){mask, *x3pio_layout_packed(X3PIO_TYPE_UINT8)};
    CHECK_OK(x3pio_read_data(reader, &into, total));

    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(x3pio_writer_set_storage(writer, storage));
    CHECK_OK(create_in_buffer(writer, copy, out));
    x3pio_const_fields from;
    x3pio_const_fields_init(&from);
    x3pio_const_field *targets[3] = {&from.x, &from.y, &from.z};
    for (size_t k = 0; k < 3; ++k)
    {
        if (stored[k] != NULL)
        {
            const x3pio_axis axis =
                x3pio_document_axis(doc, (x3pio_coordinate)k);
            *targets[k] = (x3pio_const_field){
                stored[k], *x3pio_layout_packed(axis.storage)};
        }
    }
    from.valid =
        (x3pio_const_field){mask, *x3pio_layout_packed(X3PIO_TYPE_UINT8)};
    CHECK_OK(x3pio_write_data(writer, &from, total));
    /* The files of vendors travel with the file, in the same revision. */
    if (revision == x3pio_document_revision(doc))
    {
        /* The identifiers of the file come first: in 2017 the first one fixes
         * the vendor, and in Amd 1 an identifier takes its file below. */
        for (size_t i = 0; i < x3pio_document_vendor_count(doc); ++i)
        {
            CHECK_OK(
                x3pio_add_vendor_id(writer, x3pio_document_vendor_id(doc, i)));
        }
        for (size_t i = 0; i < x3pio_reader_vendor_file_count(reader); ++i)
        {
            const char *name = x3pio_reader_vendor_file_member(reader, i);
            if (name == NULL)
            {
                continue;
            }
            size_t size = 0;
            (void)x3pio_read_member(reader, name, NULL, 0, &size);
            unsigned char *content = malloc(size != 0 ? size : 1);
            CHECK_OK(x3pio_read_member(reader, name, content, size, &size));
            /* The same call serves both revisions. */
            CHECK_OK(x3pio_add_vendor_file_by_id(
                writer, x3pio_reader_vendor_file_id(reader, i), content, size,
                NULL));
            free(content);
        }
    }
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);
    x3pio_document_free(copy);
    for (size_t k = 0; k < 3; ++k)
    {
        free(stored[k]);
    }
    free(mask);
    return true;
}

static void check_sample(const char *base, const expectation *e,
                         bool rewrite_it)
{
    char path[1100];
    snprintf(path, sizeof path, "%s/%s", base, e->path);
    const int before = failures;
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    const x3pio_status opened = x3pio_open_file(reader, path);
    CHECK_STATUS(X3PIO_OK, opened);
    if (opened != X3PIO_OK)
    {
        fprintf(stderr, "  ^ %s: %s\n", e->path,
                x3pio_reader_error_message(reader));
        x3pio_reader_free(reader);
        return;
    }
    const x3pio_document *doc = x3pio_reader_document(reader);
    CHECK_STRING(feature_name(x3pio_document_feature(doc)), e->feature);
    CHECK(x3pio_document_dimensions(doc).size_z == e->layers);
    char shape[32];
    shape_of(doc, shape, sizeof shape);
    CHECK_STRING(shape, e->shape);

    points p = {0};
    read_global(reader, &p);
    size_t valid = 0;
    double sum_x = 0;
    double sum_y = 0;
    double sum_z = 0;
    double z_first = NAN;
    double z_last = NAN;
    for (size_t i = 0; i < p.count; ++i)
    {
        if (!point_valid(&p, i))
        {
            continue;
        }
        ++valid;
        sum_x += p.x[i];
        sum_y += p.y[i];
        sum_z += p.z[i];
        z_first = isnan(z_first) ? p.z[i] : z_first;
        z_last = p.z[i];
    }
    CHECK(valid == e->valid);
    /* The sums add many numbers in an order of their own. */
    const double scale = (double)(p.count != 0 ? p.count : 1);
    CHECK(fabs(sum_x - e->sum_x) <= 1e-9 * (fabs(e->sum_x) + 1e-12 * scale));
    CHECK(fabs(sum_y - e->sum_y) <= 1e-9 * (fabs(e->sum_y) + 1e-12 * scale));
    CHECK(fabs(sum_z - e->sum_z) <= 1e-9 * (fabs(e->sum_z) + 1e-12 * scale));
    CHECK(near_relative(z_first, e->z_first, 1e-9));
    CHECK(near_relative(z_last, e->z_last, 1e-9));
    x3pio_reader_free(reader);

    if (rewrite_it)
    {
        for (int r = 0; r < 2; ++r)
        {
            for (int t = 0; t < 2; ++t)
            {
                const x3pio_revision revision =
                    r == 0 ? X3PIO_REVISION_AMD1 : X3PIO_REVISION_2017;
                const x3pio_storage storage =
                    t == 0 ? X3PIO_STORAGE_BINARY : X3PIO_STORAGE_TEXT;
                CHECK_OK(x3pio_reader_create(&reader));
                CHECK_OK(x3pio_open_file(reader, path));
                x3pio_buffer out;
                x3pio_buffer_init(&out, x3pio_default_allocator());
                rewrite(reader, revision, storage, &out);
                x3pio_reader_free(reader);

                archive a = {0};
                CHECK(archive_unzip(&a, out.data, out.size));
                member *main = archive_find(&a, "main.xml");
                char error[200] = "";
                CHECK(main != NULL
                      && xsd_validate(schema_of(revision), main->data.data,
                                      main->data.size, error, sizeof error));
                if (error[0] != '\0')
                {
                    fprintf(stderr, "  ^ %s: %s\n", e->path, error);
                }
                archive_free(&a);

                x3pio_reader *copy = NULL;
                CHECK_OK(x3pio_reader_create(&copy));
                CHECK_OK(x3pio_open_memory(copy, out.data, out.size));
                points q = {0};
                read_global(copy, &q);
                CHECK(q.count == p.count);
                for (size_t i = 0; i < q.count && i < p.count; ++i)
                {
                    CHECK_SAME(q.x[i], p.x[i]);
                    CHECK_SAME(q.y[i], p.y[i]);
                    CHECK_SAME(q.z[i], p.z[i]);
                }
                points_free(&q);
                x3pio_reader_free(copy);
                x3pio_buffer_free(&out);
            }
        }
    }
    points_free(&p);
    if (failures != before)
    {
        fprintf(stderr, "  ^ in: %s\n", e->path);
    }
}

static void test_expectations(const char *base, const char *expected,
                              bool rewrite_it, size_t minimum)
{
    expectation *list = NULL;
    const size_t count = load_expectations(expected, &list);
    CHECK(count >= minimum);
    for (size_t i = 0; i < count; ++i)
    {
        check_sample(base, &list[i], rewrite_it);
    }
    free(list);
}

/* Every sample file has an expectation, and every expectation a file. */
static void test_expectations_are_complete(void)
{
#ifdef _WIN32
    return;
#else
    expectation *list = NULL;
    const size_t count = load_expectations(test_path("expected.txt"), &list);
    size_t found = 0;
    const char *folders[] = {"opengps", "samples/iso25178-72-2017",
                             "samples/iso25178-72-fdam1"};
    for (size_t f = 0; f < 3; ++f)
    {
        DIR *dir = opendir(test_path(folders[f]));
        CHECK(dir != NULL);
        if (dir == NULL)
        {
            continue;
        }
        struct dirent *entry = NULL;
        while ((entry = readdir(dir)) != NULL)
        {
            const size_t length = strlen(entry->d_name);
            if (length < 4 || strcmp(entry->d_name + length - 4, ".x3p") != 0)
            {
                continue;
            }
            char relative[300];
            snprintf(relative, sizeof relative, "%s/%s", folders[f],
                     entry->d_name);
            bool has = false;
            for (size_t i = 0; i < count; ++i)
            {
                has = has || strcmp(list[i].path, relative) == 0;
            }
            CHECK(has);
            if (!has)
            {
                fprintf(stderr, "  ^ no expectation for %s\n", relative);
            }
            ++found;
        }
        closedir(dir);
    }
    CHECK(found == count);
    free(list);
#endif
}

/* The main.xml example of Amd 1, wrapped in a container. */
static void test_standard_example(void)
{
    size_t size = 0;
    unsigned char *xml = read_test_file("opengps/iso25178-72-fdam1.xml", &size);
    CHECK(xml != NULL);
    if (xml == NULL)
    {
        return;
    }
    char error[200] = "";
    CHECK(
        xsd_validate("iso25178-72-fdam1.xsd", xml, size, error, sizeof error));
    archive a = {0};
    archive_put(&a, "main.xml", xml, size);
    archive_fix_checksum(&a);
    x3pio_buffer zip;
    x3pio_buffer_init(&zip, x3pio_default_allocator());
    CHECK(archive_zip(&a, &zip));
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, zip.data, zip.size));
    const x3pio_document *doc = x3pio_reader_document(reader);
    CHECK(doc != NULL && x3pio_document_revision(doc) == X3PIO_REVISION_AMD1);
    CHECK(x3pio_document_feature(doc) == X3PIO_SURFACE);
    const x3pio_dimensions dims = x3pio_document_dimensions(doc);
    CHECK(dims.size_x == 4 && dims.size_y == 4 && dims.size_z == 1);
    CHECK(equals(x3pio_document_axis(doc, X3PIO_X).increment, 1.6016e-6));
    const x3pio_transformation place = x3pio_document_transformation(doc);
    CHECK(!x3pio_transformation_has_rotation(&place));
    CHECK_STRING(x3pio_document_creator(doc), "Name of measuring person");
    CHECK_STRING(x3pio_document_serial(doc), "12345abc");
    CHECK(x3pio_document_probing_type(doc) == X3PIO_PROBING_NON_CONTACTING);
    CHECK_STRING(x3pio_document_comment(doc),
                 "This is a user comment specific to this data set");
    x3pio_timestamp date;
    CHECK(x3pio_document_date(doc, &date));
    char text[64];
    x3pio_timestamp_format(&date, text, sizeof text);
    CHECK_STRING(text, "2007-04-30T13:58:02.6+02:00");
    double z[16];
    CHECK_OK(x3pio_read_z(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
    CHECK(equals(z[0], 4.86219120804151E-0006));
    CHECK(isnan(z[7]));
    x3pio_reader_free(reader);
    archive_free(&a);
    x3pio_buffer_free(&zip);
    free(xml);
}

int main(void)
{
    test_expectations(X3PIO_TEST_DATA_DIR, test_path("expected.txt"), true, 30);
    test_expectations_are_complete();
    test_standard_example();

    /* Larger files of the developer, if the environment names a folder with an
     * expected.txt created by create_expected.py. */
    const char *local = getenv("X3PIO_LOCAL_SAMPLES");
    if (local != NULL && local[0] != '\0')
    {
        char expected[1100];
        snprintf(expected, sizeof expected, "%s/expected.txt", local);
        test_expectations(local, expected, false, 1);
    }
    else
    {
        printf("X3PIO_LOCAL_SAMPLES is not set: larger files skipped\n");
    }
    return finish_tests();
}
