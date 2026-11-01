/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* What the writer makes, the reader reads back: every shape of file in both
 * revisions and both storages, valid against the schemas, in the three layers
 * and into the memory of the program in the ways a program holds its data. */

#include "codec.h"
#include "helpers.h"
#include "scenarios.h"

#include <stdint.h>
#include <string.h>

static const char *schema_of(x3pio_revision revision)
{
    return revision == X3PIO_REVISION_AMD1 ? "iso25178-72-fdam1.xsd"
                                           : "iso25178-72-2017.xsd";
}

/* The fields of a scenario in metres: x and y only where the file stores them.
 */
static void bind_metres(const scenario *s, const double *x, const double *y,
                        const double *z, x3pio_const_fields *out)
{
    x3pio_const_fields_init(out);
    out->z = (x3pio_const_field){z, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    if (scenario_stores_x(s))
    {
        out->x =
            (x3pio_const_field){x, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    }
    if (scenario_stores_y(s))
    {
        out->y =
            (x3pio_const_field){y, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
    }
}

/* Writes the points of a scenario to a buffer, in metres. */
static void write_scenario(const scenario *s, x3pio_revision revision,
                           x3pio_storage storage, x3pio_buffer *out)
{
    const size_t count = scenario_points(s);
    double *x = malloc(count * sizeof *x);
    double *y = malloc(count * sizeof *y);
    double *z = malloc(count * sizeof *z);
    scenario_points_view(s, x, y, z);
    x3pio_document *doc = scenario_document(s, revision);
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    CHECK_OK(x3pio_writer_set_storage(writer, storage));
    CHECK_OK(create_in_buffer(writer, doc, out));
    x3pio_const_fields from;
    bind_metres(s, x, y, z, &from);
    CHECK_OK(x3pio_write_points(writer, &from, scenario_rows(s)));
    CHECK_OK(x3pio_end_file(writer));
    x3pio_writer_free(writer);
    x3pio_document_free(doc);
    free(x);
    free(y);
    free(z);
}

/* Reads the three coordinates of every point in metres. */
// NOLINTNEXTLINE(readability-non-const-parameter)
static void read_metres(x3pio_reader *reader, double *x, double *y, double *z)
{
    const x3pio_document *doc = x3pio_reader_document(reader);
    x3pio_fields into;
    x3pio_fields_init(&into);
    const x3pio_layout *packed = x3pio_layout_packed(X3PIO_TYPE_DOUBLE);
    into.x = (x3pio_field){x, *packed};
    into.y = (x3pio_field){y, *packed};
    into.z = (x3pio_field){z, *packed};
    CHECK_OK(x3pio_read_points(reader, &into, x3pio_document_rows(doc)));
}

static void check_scenario(const scenario *s, x3pio_revision revision,
                           x3pio_storage storage)
{
    char label[200];
    snprintf(label, sizeof label, "%s, %s, %s", s->name,
             revision == X3PIO_REVISION_AMD1 ? "Amd 1" : "2017",
             storage == X3PIO_STORAGE_BINARY ? "binary" : "text");
    const int before = failures;
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    write_scenario(s, revision, storage, &bytes);

    /* The container, taken apart without the library. */
    archive a = {0};
    CHECK(archive_unzip(&a, bytes.data, bytes.size));
    const bool integer =
        s->type == X3PIO_TYPE_INT16 || s->type == X3PIO_TYPE_INT32;
    CHECK(archive_find(&a, "main.xml") != NULL);
    CHECK(archive_find(&a, "md5checksum.hex") != NULL);
    CHECK((archive_find(&a, "bindata/data.bin") != NULL)
          == (storage == X3PIO_STORAGE_BINARY));
    CHECK((archive_find(&a, "bindata/valid.bin") != NULL)
          == (storage == X3PIO_STORAGE_BINARY && s->missing && integer));
    member *main = archive_find(&a, "main.xml");
    char error[200] = "";
    if (main != NULL)
    {
        CHECK(xsd_validate(schema_of(revision), main->data.data,
                           main->data.size, error, sizeof error));
        if (error[0] != '\0')
        {
            fprintf(stderr, "%s: %s\n", label, error);
        }
    }
    const size_t record = (1U + (scenario_stores_x(s) ? 1U : 0U)
                           + (scenario_stores_y(s) ? 1U : 0U))
                          * x3pio_type_size(s->type);
    member *data = archive_find(&a, "bindata/data.bin");
    if (data != NULL)
    {
        CHECK(data->data.size == scenario_points(s) * record);
    }

    /* The values read back are those of the stored type. */
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    const x3pio_document *doc = x3pio_reader_document(reader);
    CHECK(doc != NULL && x3pio_document_feature(doc) == s->feature);
    CHECK(doc != NULL && x3pio_document_revision(doc) == revision);
    if (doc != NULL)
    {
        const x3pio_dimensions dims = x3pio_document_dimensions(doc);
        CHECK(dims.size_x == s->columns && dims.size_y == s->rows
              && dims.size_z == s->layers
              && dims.list == (s->feature == X3PIO_POINT_CLOUD));
        CHECK(x3pio_document_columns(doc)
              == (s->feature == X3PIO_POINT_CLOUD ? 1 : s->columns));
        CHECK(x3pio_document_rows(doc) == scenario_rows(s));
        CHECK((x3pio_document_axis(doc, X3PIO_X).type == X3PIO_AXIS_INCREMENTAL)
              == !scenario_stores_x(s));
        CHECK((x3pio_document_axis(doc, X3PIO_Y).type == X3PIO_AXIS_INCREMENTAL)
              == !scenario_stores_y(s));
        CHECK(x3pio_document_axis(doc, X3PIO_Z).storage == s->type);
        CHECK(x3pio_document_has_metadata(doc));
        CHECK_STRING(x3pio_document_model(doc), s->name);
        CHECK(x3pio_document_has_validity(doc)
              == ((storage == X3PIO_STORAGE_BINARY ? integer : true)
                  && s->missing && (integer || storage == X3PIO_STORAGE_TEXT)));
    }
    const size_t count = scenario_points(s);
    double *x = malloc(count * sizeof *x);
    double *y = malloc(count * sizeof *y);
    double *z = malloc(count * sizeof *z);
    double *ex = malloc(count * sizeof *ex);
    double *ey = malloc(count * sizeof *ey);
    double *ez = malloc(count * sizeof *ez);
    scenario_points_view(s, ex, ey, ez);
    read_metres(reader, x, y, z);
    const double tolerance = scenario_tolerance(s);
    const double x_tolerance = scenario_stores_x(s) ? tolerance : 1e-20;
    const double y_tolerance = scenario_stores_y(s) ? tolerance : 1e-20;
    for (size_t i = 0; i < count; ++i)
    {
        if (scenario_is_missing(s, i))
        {
            CHECK(isnan(z[i]));
            if (scenario_stores_x(s))
            {
                CHECK(isnan(x[i]));
            }
            if (scenario_stores_y(s))
            {
                CHECK(isnan(y[i]));
            }
            continue;
        }
        const double want_z = scenario_stored(s->type, s->increment, ez[i]);
        const double want_x =
            scenario_stores_x(s) ? scenario_stored(s->type, s->increment, ex[i])
                                 : ex[i];
        const double want_y =
            scenario_stores_y(s) ? scenario_stored(s->type, s->increment, ey[i])
                                 : ey[i];
        CHECK_NEAR(z[i], want_z, tolerance + fabs(want_z) * 1e-12);
        CHECK_NEAR(x[i], want_x, x_tolerance + fabs(want_x) * 1e-12);
        CHECK_NEAR(y[i], want_y, y_tolerance + fabs(want_y) * 1e-12);
    }

    /* The transformation of the document puts the points in the global system.
     */
    const x3pio_transformation place = scenario_transformation(s);
    const x3pio_transformation read_place = x3pio_document_transformation(doc);
    for (size_t k = 0; k < 9; ++k)
    {
        CHECK_NEAR(read_place.rotation[k], place.rotation[k], 1e-12);
    }
    for (size_t k = 0; k < 3; ++k)
    {
        CHECK_NEAR(read_place.offset[k], place.offset[k], 1e-15);
    }
    for (size_t i = 0; i < count; ++i)
    {
        if (scenario_is_missing(s, i))
        {
            continue;
        }
        const double view[3] = {x[i], y[i], z[i]};
        double global[3];
        double again[3];
        x3pio_transformation_to_global(&read_place, view, global);
        x3pio_transformation_to_view(&read_place, global, again);
        CHECK_NEAR(again[0], x[i], 1e-15);
        CHECK_NEAR(again[1], y[i], 1e-15);
        CHECK_NEAR(again[2], z[i], 1e-15);
    }
    x3pio_reader_free(reader);

    /* The stored values: the numbers of the file in the type of the axis, with
     * the mask of the points. */
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    unsigned char *stored_z = malloc(count * 8);
    unsigned char *stored_x = malloc(count * 8);
    unsigned char *stored_y = malloc(count * 8);
    unsigned char *mask = malloc(count);
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.z = (x3pio_field){stored_z, *x3pio_layout_packed(s->type)};
    into.valid = (x3pio_field){mask, *x3pio_layout_packed(X3PIO_TYPE_UINT8)};
    if (scenario_stores_x(s))
    {
        into.x = (x3pio_field){stored_x, *x3pio_layout_packed(s->type)};
    }
    if (scenario_stores_y(s))
    {
        into.y = (x3pio_field){stored_y, *x3pio_layout_packed(s->type)};
    }
    CHECK_OK(x3pio_read_data(reader, &into, scenario_rows(s)));
    for (size_t i = 0; i < count; ++i)
    {
        CHECK(mask[i] == (scenario_is_missing(s, i) ? 0 : 1));
        if (scenario_is_missing(s, i))
        {
            continue;
        }
        double got = 0;
        x3pio_matrix m;
        CHECK(x3pio_matrix_make(&m, stored_z, x3pio_layout_packed(s->type),
                                count, 1, false));
        x3pio_matrix_load(&m, i, &got, 1);
        const double want = ez[i] / s->increment;
        CHECK_NEAR(got, want, integer ? 0.5 : fabs(want) * 1e-6 + 1e-12);
        if (integer)
        {
            CHECK(got == floor(got));
        }
    }
    free(stored_x);
    free(stored_y);
    free(stored_z);
    free(mask);
    x3pio_reader_free(reader);
    archive_free(&a);
    x3pio_buffer_free(&bytes);
    free(x);
    free(y);
    free(z);
    free(ex);
    free(ey);
    free(ez);
    if (failures != before)
    {
        fprintf(stderr, "  ^ in: %s\n", label);
    }
}

static void test_every_shape(void)
{
    for (size_t i = 0; i < SCENARIO_COUNT; ++i)
    {
        for (int r = 0; r < 2; ++r)
        {
            for (int t = 0; t < 2; ++t)
            {
                check_scenario(
                    &scenarios[i],
                    r == 0 ? X3PIO_REVISION_AMD1 : X3PIO_REVISION_2017,
                    t == 0 ? X3PIO_STORAGE_BINARY : X3PIO_STORAGE_TEXT);
            }
        }
    }
}

/* A file written again from the stored values it holds is the same. */
static void test_rewrite(void)
{
    for (size_t i = 0; i < SCENARIO_COUNT; ++i)
    {
        for (int t = 0; t < 2; ++t)
        {
            const scenario *s = &scenarios[i];
            const x3pio_storage storage =
                t == 0 ? X3PIO_STORAGE_BINARY : X3PIO_STORAGE_TEXT;
            x3pio_buffer first;
            x3pio_buffer_init(&first, x3pio_default_allocator());
            write_scenario(s, X3PIO_REVISION_AMD1, storage, &first);
            x3pio_reader *reader = NULL;
            CHECK_OK(x3pio_reader_create(&reader));
            CHECK_OK(x3pio_open_memory(reader, first.data, first.size));
            const x3pio_document *doc = x3pio_reader_document(reader);
            const size_t count = x3pio_document_point_count(doc);
            const size_t rows = x3pio_document_rows(doc);
            unsigned char *sx = malloc(count * 8);
            unsigned char *sy = malloc(count * 8);
            unsigned char *sz = malloc(count * 8);
            unsigned char *mask = malloc(count);
            x3pio_fields into;
            x3pio_fields_init(&into);
            into.z = (x3pio_field){sz, *x3pio_layout_packed(s->type)};
            into.valid =
                (x3pio_field){mask, *x3pio_layout_packed(X3PIO_TYPE_UINT8)};
            if (scenario_stores_x(s))
            {
                into.x = (x3pio_field){sx, *x3pio_layout_packed(s->type)};
            }
            if (scenario_stores_y(s))
            {
                into.y = (x3pio_field){sy, *x3pio_layout_packed(s->type)};
            }
            CHECK_OK(x3pio_read_data(reader, &into, rows));

            x3pio_writer *writer = NULL;
            CHECK_OK(x3pio_writer_create(&writer));
            CHECK_OK(x3pio_writer_set_storage(writer, storage));
            x3pio_buffer second;
            x3pio_buffer_init(&second, x3pio_default_allocator());
            CHECK_OK(create_in_buffer(writer, doc, &second));
            x3pio_const_fields from;
            x3pio_const_fields_init(&from);
            from.z = (x3pio_const_field){sz, *x3pio_layout_packed(s->type)};
            from.valid = (x3pio_const_field){
                mask, *x3pio_layout_packed(X3PIO_TYPE_UINT8)};
            if (scenario_stores_x(s))
            {
                from.x = (x3pio_const_field){sx, *x3pio_layout_packed(s->type)};
            }
            if (scenario_stores_y(s))
            {
                from.y = (x3pio_const_field){sy, *x3pio_layout_packed(s->type)};
            }
            CHECK_OK(x3pio_write_data(writer, &from, rows));
            CHECK_OK(x3pio_end_file(writer));
            /* Nothing changed: the same bytes. */
            const bool same =
                second.size == first.size
                && memcmp(second.data, first.data, first.size) == 0;
            CHECK(same);
            if (!same)
            {
                fprintf(stderr, "  ^ rewrite of %s, %s\n", s->name,
                        t == 0 ? "binary" : "text");
            }
            x3pio_writer_free(writer);
            x3pio_reader_free(reader);
            x3pio_buffer_free(&first);
            x3pio_buffer_free(&second);
            free(sx);
            free(sy);
            free(sz);
            free(mask);
        }
    }
}

/* ---- the memory of the program
 * ------------------------------------------------- */

static const scenario *find_scenario(const char *name)
{
    for (size_t i = 0; i < SCENARIO_COUNT; ++i)
    {
        if (strcmp(scenarios[i].name, name) == 0)
        {
            return &scenarios[i];
        }
    }
    return NULL;
}

static x3pio_reader *open_scenario(const scenario *s, x3pio_revision revision,
                                   x3pio_storage storage, x3pio_buffer *keep)
{
    write_scenario(s, revision, storage, keep);
    x3pio_reader *reader = NULL;
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, keep->data, keep->size));
    return reader;
}

static void test_image_layouts(void)
{
    const scenario *s = find_scenario("surface float with missing points");
    const size_t columns = s->columns;
    const size_t rows = s->rows;
    const size_t count = columns * rows;
    double *ez = malloc(count * sizeof *ez);
    double *ex = malloc(count * sizeof *ex);
    double *ey = malloc(count * sizeof *ey);
    scenario_points_view(s, ex, ey, ez);
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    x3pio_reader *reader =
        open_scenario(s, X3PIO_REVISION_AMD1, X3PIO_STORAGE_BINARY, &bytes);

    /* A depth image of floats: the heights of the matrix as they are. */
    float *image = malloc(count * sizeof *image);
    CHECK_OK(x3pio_read_heights(reader, x3pio_layout_packed(X3PIO_TYPE_FLOAT),
                                image));
    for (size_t i = 0; i < count; ++i)
    {
        if (scenario_is_missing(s, i))
        {
            CHECK(isnan(image[i]));
        }
        else
        {
            CHECK_NEAR(image[i], ez[i], 1e-6 * fabs(ez[i]) + 1e-13);
        }
    }
    x3pio_reader_free(reader);

    /* Read again into other layouts, each from a reader of its own: the rows of
     * a file are read once. */
    const size_t pitch = columns + 5;
    float *padded = malloc(rows * pitch * sizeof *padded);
    for (size_t i = 0; i < rows * pitch; ++i)
    {
        padded[i] = -777.0F;
    }
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    x3pio_layout layout = {X3PIO_TYPE_FLOAT, 0,
                           (ptrdiff_t)(pitch * sizeof(float))};
    CHECK_OK(x3pio_read_heights(reader, &layout, padded));
    for (size_t r = 0; r < rows; ++r)
    {
        for (size_t col = 0; col < columns; ++col)
        {
            CHECK_SAME(padded[r * pitch + col], image[r * columns + col]);
        }
        for (size_t col = columns; col < pitch; ++col)
        {
            CHECK(padded[r * pitch + col] == -777.0F);
        }
    }
    x3pio_reader_free(reader);

    /* Upside down: the last row of memory is the first row of the file. */
    float *flipped = malloc(count * sizeof *flipped);
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    layout = (x3pio_layout){X3PIO_TYPE_FLOAT, 0,
                            -(ptrdiff_t)(columns * sizeof(float))};
    CHECK_OK(
        x3pio_read_heights(reader, &layout, flipped + (rows - 1) * columns));
    for (size_t r = 0; r < rows; ++r)
    {
        for (size_t col = 0; col < columns; ++col)
        {
            CHECK_SAME(flipped[(rows - 1 - r) * columns + col],
                       image[r * columns + col]);
        }
    }
    x3pio_reader_free(reader);

    /* Column-major: the matrix of a library that stores columns. */
    float *transposed = malloc(count * sizeof *transposed);
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    layout = (x3pio_layout){X3PIO_TYPE_FLOAT, (ptrdiff_t)(rows * sizeof(float)),
                            (ptrdiff_t)sizeof(float)};
    CHECK_OK(x3pio_read_heights(reader, &layout, transposed));
    for (size_t r = 0; r < rows; ++r)
    {
        for (size_t col = 0; col < columns; ++col)
        {
            CHECK_SAME(transposed[col * rows + r], image[r * columns + col]);
        }
    }
    x3pio_reader_free(reader);

    /* Wrong buffers are refused, and nothing is touched. */
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    x3pio_fields into;
    x3pio_fields_init(&into);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, &into, rows));
    CHECK_STRING(x3pio_reader_error_message(reader),
                 "No field is bound to read into");
    into.z = (x3pio_field){image, {X3PIO_TYPE_INT32, 0, 0}};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, &into, rows));
    into.z = (x3pio_field){image, {(x3pio_type)42, 0, 0}};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, &into, rows));
    into.z = (x3pio_field){image, {X3PIO_TYPE_FLOAT, 0, 0}};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_read_points(reader, &into, rows + 1));
    CHECK_STRING(x3pio_reader_error_message(reader),
                 "More rows than the file has left");
    unsigned char spare[1];
    into.valid = (x3pio_field){spare, {X3PIO_TYPE_UINT8, 0, 0}};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, &into, rows));
    x3pio_fields_init(&into);
    /* The stored values are read in the type of the axis, and only the axes the
     * file stores are bound. */
    into.z = (x3pio_field){image, {X3PIO_TYPE_DOUBLE, 0, 0}};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_data(reader, &into, rows));
    into.z = (x3pio_field){image, {X3PIO_TYPE_FLOAT, 0, 0}};
    into.x = (x3pio_field){image, {X3PIO_TYPE_FLOAT, 0, 0}};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_data(reader, &into, rows));
    /* Strides that put the elements on top of each other. */
    into.x = (x3pio_field){NULL, {X3PIO_TYPE_FLOAT, 0, 0}};
    x3pio_layout_init(&layout);
    layout.type = X3PIO_TYPE_FLOAT;
    layout.element_stride = 0;
    /* The reader works after these mistakes. */
    CHECK_OK(x3pio_read_heights(reader, x3pio_layout_packed(X3PIO_TYPE_FLOAT),
                                image));
    /* The rows of a file are read once: a second read has nothing left. */
    CHECK_STATUS(X3PIO_ERR_STATE,
                 x3pio_read_heights(
                     reader, x3pio_layout_packed(X3PIO_TYPE_FLOAT), image));
    into.z = (x3pio_field){image, {X3PIO_TYPE_FLOAT, 0, 0}};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, &into, 1));
    CHECK_OK(x3pio_read_points(reader, &into, 0));
    x3pio_reader_free(reader);

    free(image);
    free(padded);
    free(flipped);
    free(transposed);
    free(ex);
    free(ey);
    free(ez);
    x3pio_buffer_free(&bytes);
}

/* A structure of the program, with padding and other members around the
 * coordinates. */
typedef struct sample_point
{
    char tag;
    float x;
    short intensity;
    float y;
    double z;
    char tail;
} sample_point;

static void test_structures(void)
{
    const scenario *s = find_scenario("point cloud double");
    const size_t count = s->columns;
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    x3pio_reader *reader =
        open_scenario(s, X3PIO_REVISION_AMD1, X3PIO_STORAGE_BINARY, &bytes);
    double *ex = malloc(count * sizeof *ex);
    double *ey = malloc(count * sizeof *ey);
    double *ez = malloc(count * sizeof *ez);
    scenario_points_view(s, ex, ey, ez);

    /* Metres into the members of the structures, in the types of the program.
     */
    sample_point *points = malloc(count * sizeof *points);
    for (size_t i = 0; i < count; ++i)
    {
        points[i] = (sample_point){'a', -1.0F, 77, -1.0F, -1.0, 'z'};
    }
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.x = (x3pio_field){&points[0].x, {X3PIO_TYPE_FLOAT, sizeof *points, 0}};
    into.y = (x3pio_field){&points[0].y, {X3PIO_TYPE_FLOAT, sizeof *points, 0}};
    into.z =
        (x3pio_field){&points[0].z, {X3PIO_TYPE_DOUBLE, sizeof *points, 0}};
    CHECK_OK(x3pio_read_points(reader, &into, count));
    for (size_t i = 0; i < count; ++i)
    {
        CHECK(points[i].tag == 'a' && points[i].tail == 'z'
              && points[i].intensity == 77);
        CHECK(points[i].x == (float)ex[i]);
        CHECK(points[i].y == (float)ey[i]);
        CHECK(points[i].z == ez[i]);
    }
    x3pio_reader_free(reader);

    /* Written from the structures again, into a file of floats. */
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(&doc));
    CHECK_OK(x3pio_document_set_feature_point_cloud(doc, count,
                                                    X3PIO_TYPE_FLOAT, 1.0));
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer second;
    x3pio_buffer_init(&second, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &second));
    x3pio_const_fields from;
    x3pio_const_fields_init(&from);
    from.x = (x3pio_const_field){&points[0].x,
                                 {X3PIO_TYPE_FLOAT, sizeof *points, 0}};
    from.y = (x3pio_const_field){&points[0].y,
                                 {X3PIO_TYPE_FLOAT, sizeof *points, 0}};
    from.z = (x3pio_const_field){&points[0].z,
                                 {X3PIO_TYPE_DOUBLE, sizeof *points, 0}};
    CHECK_OK(x3pio_write_points(writer, &from, count));
    CHECK_OK(x3pio_end_file(writer));
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, second.data, second.size));
    double *x = malloc(count * sizeof *x);
    double *y = malloc(count * sizeof *y);
    double *z = malloc(count * sizeof *z);
    read_metres(reader, x, y, z);
    for (size_t i = 0; i < count; ++i)
    {
        CHECK(x[i] == (double)points[i].x && y[i] == (double)points[i].y
              && z[i] == (double)(float)points[i].z);
    }
    x3pio_reader_free(reader);
    x3pio_writer_free(writer);
    x3pio_document_free(doc);
    x3pio_buffer_free(&bytes);
    x3pio_buffer_free(&second);

    /* The stored values of a file into the members, in the type of the file. */
    struct stored
    {
        char tag;
        int16_t x;
        int16_t y;
        int16_t z;
        char tail;
    };
    const scenario *ints = find_scenario("point cloud int16");
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    reader =
        open_scenario(ints, X3PIO_REVISION_AMD1, X3PIO_STORAGE_BINARY, &bytes);
    struct stored *records = malloc(count * sizeof *records);
    for (size_t i = 0; i < count; ++i)
    {
        records[i] = (struct stored){'a', 0, 0, 0, 'z'};
    }
    x3pio_fields_init(&into);
    into.x =
        (x3pio_field){&records[0].x, {X3PIO_TYPE_INT16, sizeof *records, 0}};
    into.y =
        (x3pio_field){&records[0].y, {X3PIO_TYPE_INT16, sizeof *records, 0}};
    into.z =
        (x3pio_field){&records[0].z, {X3PIO_TYPE_INT16, sizeof *records, 0}};
    CHECK_OK(x3pio_read_data(reader, &into, count));
    for (size_t i = 0; i < count; ++i)
    {
        CHECK(records[i].tag == 'a' && records[i].tail == 'z');
        CHECK(records[i].x == (int16_t)nearbyint(ex[i] / ints->increment));
        CHECK(records[i].y == (int16_t)nearbyint(ey[i] / ints->increment));
        CHECK(records[i].z == (int16_t)nearbyint(ez[i] / ints->increment));
    }
    x3pio_reader_free(reader);
    x3pio_buffer_free(&bytes);
    free(records);
    free(points);
    free(x);
    free(y);
    free(z);
    free(ex);
    free(ey);
    free(ez);
}

/* The layers of a file are rows that follow each other: an image is read or
 * written by as many rows as it has. */
static void test_layers(void)
{
    const scenario *s = find_scenario("surface in layers int16");
    const size_t columns = s->columns;
    const size_t rows = s->rows;
    const size_t layer_points = columns * rows;
    const size_t count = layer_points * s->layers;
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    x3pio_reader *reader =
        open_scenario(s, X3PIO_REVISION_AMD1, X3PIO_STORAGE_BINARY, &bytes);
    double *ex = malloc(count * sizeof *ex);
    double *ey = malloc(count * sizeof *ey);
    double *ez = malloc(count * sizeof *ez);
    scenario_points_view(s, ex, ey, ez);

    /* One image of floats for each layer, separately allocated. */
    float *images[3];
    for (size_t l = 0; l < 3; ++l)
    {
        images[l] = malloc(layer_points * sizeof(float));
        x3pio_fields into;
        x3pio_fields_init(&into);
        into.z =
            (x3pio_field){images[l], *x3pio_layout_packed(X3PIO_TYPE_FLOAT)};
        CHECK_OK(x3pio_read_points(reader, &into, rows));
    }
    /* All the rows are read. */
    float spare[4];
    x3pio_fields more;
    x3pio_fields_init(&more);
    more.z = (x3pio_field){spare, *x3pio_layout_packed(X3PIO_TYPE_FLOAT)};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_read_points(reader, &more, 1));
    for (size_t l = 0; l < 3; ++l)
    {
        for (size_t i = 0; i < layer_points; ++i)
        {
            const size_t at = l * layer_points + i;
            if (scenario_is_missing(s, at))
            {
                CHECK(isnan(images[l][i]));
            }
            else
            {
                CHECK_NEAR(images[l][i],
                           scenario_stored(s->type, s->increment, ez[at]),
                           1e-9);
            }
        }
    }
    x3pio_reader_free(reader);

    /* The whole stack in one buffer of 3 * rows rows. */
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
    float *stack = malloc(count * sizeof *stack);
    CHECK_OK(x3pio_read_heights(reader, x3pio_layout_packed(X3PIO_TYPE_FLOAT),
                                stack));
    for (size_t l = 0; l < 3; ++l)
    {
        for (size_t i = 0; i < layer_points; ++i)
        {
            CHECK_SAME(stack[l * layer_points + i], images[l][i]);
        }
    }
    x3pio_reader_free(reader);

    /* Written layer by layer into a file of floats. */
    x3pio_document *doc = scenario_document(s, X3PIO_REVISION_AMD1);
    CHECK_OK(x3pio_document_set_feature_surface(
        doc, columns, rows, GRID_STEP, GRID_STEP, X3PIO_TYPE_FLOAT, 1.0, 3));
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer second;
    x3pio_buffer_init(&second, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &second));
    for (size_t l = 0; l < 3; ++l)
    {
        x3pio_const_fields from;
        x3pio_const_fields_init(&from);
        from.z = (x3pio_const_field){images[l],
                                     *x3pio_layout_packed(X3PIO_TYPE_FLOAT)};
        CHECK_OK(x3pio_write_points(writer, &from, rows));
    }
    /* All rows are written. */
    x3pio_const_fields extra;
    x3pio_const_fields_init(&extra);
    extra.z =
        (x3pio_const_field){images[0], *x3pio_layout_packed(X3PIO_TYPE_FLOAT)};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_write_points(writer, &extra, 1));
    CHECK_OK(x3pio_end_file(writer));
    CHECK_OK(x3pio_reader_create(&reader));
    CHECK_OK(x3pio_open_memory(reader, second.data, second.size));
    float *back = malloc(count * sizeof *back);
    CHECK_OK(x3pio_read_heights(reader, x3pio_layout_packed(X3PIO_TYPE_FLOAT),
                                back));
    for (size_t i = 0; i < count; ++i)
    {
        CHECK_SAME(back[i], stack[i]);
    }
    x3pio_reader_free(reader);
    x3pio_writer_free(writer);
    x3pio_document_free(doc);
    x3pio_buffer_free(&bytes);
    x3pio_buffer_free(&second);
    for (size_t l = 0; l < 3; ++l)
    {
        free(images[l]);
    }
    free(stack);
    free(back);
    free(ex);
    free(ey);
    free(ez);
}

/* Rows of any number, and chunks of any size, give the same points as a read of
 * all of them, and the same file. */
static void test_pieces(void)
{
    const scenario *s = find_scenario("surface int32 with missing points");
    const size_t count = scenario_points(s);
    const size_t columns = s->columns;
    const size_t rows = s->rows;
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    x3pio_reader *reader =
        open_scenario(s, X3PIO_REVISION_AMD1, X3PIO_STORAGE_BINARY, &bytes);
    double *whole = malloc(count * sizeof *whole);
    CHECK_OK(x3pio_read_heights(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE),
                                whole));
    x3pio_reader_free(reader);

    const size_t chunks[] = {1, 2, 3, 7, 64, 299, 300, 1000};
    const size_t pieces[] = {1, 2, 4, 5, 15};
    for (size_t c = 0; c < sizeof chunks / sizeof chunks[0]; ++c)
    {
        for (size_t p = 0; p < sizeof pieces / sizeof pieces[0]; ++p)
        {
            CHECK_OK(x3pio_reader_create(&reader));
            CHECK_OK(x3pio_reader_set_chunk(reader, chunks[c]));
            CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
            double *joined = malloc(count * sizeof *joined);
            for (size_t row = 0; row < rows; row += pieces[p])
            {
                const size_t n =
                    rows - row < pieces[p] ? rows - row : pieces[p];
                x3pio_fields into;
                x3pio_fields_init(&into);
                into.z = (x3pio_field){joined + row * columns,
                                       *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
                CHECK_OK(x3pio_read_points(reader, &into, n));
            }
            for (size_t i = 0; i < count; ++i)
            {
                CHECK_SAME(joined[i], whole[i]);
            }
            free(joined);
            x3pio_reader_free(reader);
        }
    }

    /* Writing in pieces of any size makes the same file as writing it at once.
     */
    x3pio_buffer once;
    x3pio_buffer_init(&once, x3pio_default_allocator());
    write_scenario(s, X3PIO_REVISION_AMD1, X3PIO_STORAGE_BINARY, &once);
    double *x = malloc(count * sizeof *x);
    double *y = malloc(count * sizeof *y);
    double *z = malloc(count * sizeof *z);
    scenario_points_view(s, x, y, z);
    archive b = {0};
    CHECK(archive_unzip(&b, once.data, once.size));
    for (size_t c = 0; c < sizeof chunks / sizeof chunks[0]; ++c)
    {
        x3pio_document *doc = scenario_document(s, X3PIO_REVISION_AMD1);
        x3pio_writer *writer = NULL;
        CHECK_OK(x3pio_writer_create(&writer));
        CHECK_OK(x3pio_writer_set_chunk(writer, chunks[c]));
        x3pio_buffer out;
        x3pio_buffer_init(&out, x3pio_default_allocator());
        CHECK_OK(create_in_buffer(writer, doc, &out));
        for (size_t row = 0; row < rows; row += 4)
        {
            const size_t n = rows - row < 4 ? rows - row : 4;
            x3pio_const_fields from;
            x3pio_const_fields_init(&from);
            from.z = (x3pio_const_field){
                z + row * columns, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
            CHECK_OK(x3pio_write_points(writer, &from, n));
        }
        CHECK_OK(x3pio_end_file(writer));
        archive a = {0};
        CHECK(archive_unzip(&a, out.data, out.size));
        for (size_t i = 0; i < b.count; ++i)
        {
            member *other = archive_find(&a, b.items[i].name);
            CHECK(other != NULL && other->data.size == b.items[i].data.size
                  && memcmp(other->data.data, b.items[i].data.data,
                            other->data.size)
                         == 0);
        }
        archive_free(&a);
        x3pio_buffer_free(&out);
        x3pio_writer_free(writer);
        x3pio_document_free(doc);
    }
    archive_free(&b);
    free(whole);
    free(x);
    free(y);
    free(z);
    x3pio_buffer_free(&bytes);
    x3pio_buffer_free(&once);
}

/* The heights of a grid are written whole, and an integer file chooses its
 * increment from them. */
static void test_heights_and_automatic_increment(void)
{
    const size_t columns = 20;
    const size_t rows = 15;
    const size_t count = columns * rows;
    double *z = malloc(count * sizeof *z);
    double highest = 0.0;
    for (size_t i = 0; i < count; ++i)
    {
        z[i] = 3.7e-6 * sin(0.37 * (double)i);
        highest = fmax(highest, fabs(z[i]));
    }
    z[17] = NAN;
    for (int t = 0; t < 2; ++t)
    {
        for (int type = 0; type < 2; ++type)
        {
            x3pio_document *doc = NULL;
            CHECK_OK(x3pio_document_create(&doc));
            const x3pio_type stored =
                type == 0 ? X3PIO_TYPE_INT16 : X3PIO_TYPE_INT32;
            CHECK_OK(x3pio_document_set_feature_surface(
                doc, columns, rows, 1e-6, 1e-6, stored, X3PIO_AUTO_INCREMENT,
                1));
            x3pio_writer *writer = NULL;
            CHECK_OK(x3pio_writer_create(&writer));
            CHECK_OK(x3pio_writer_set_storage(
                writer, t == 0 ? X3PIO_STORAGE_BINARY : X3PIO_STORAGE_TEXT));
            x3pio_buffer bytes;
            x3pio_buffer_init(&bytes, x3pio_default_allocator());
            CHECK_OK(create_in_buffer(writer, doc, &bytes));
            /* The other layers cannot choose. */
            x3pio_const_fields wrong;
            x3pio_const_fields_init(&wrong);
            wrong.z =
                (x3pio_const_field){z, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
            CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                         x3pio_write_points(writer, &wrong, rows));
            CHECK(strstr(x3pio_writer_error_message(writer), "increment")
                  != NULL);
            CHECK_OK(x3pio_write_heights(
                writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
            CHECK_OK(x3pio_end_file(writer));

            x3pio_reader *reader = NULL;
            CHECK_OK(x3pio_reader_create(&reader));
            CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
            const double increment =
                x3pio_document_axis(x3pio_reader_document(reader), X3PIO_Z)
                    .increment;
            const double want =
                x3pio_suggest_increment_for_range(stored, -highest, highest);
            CHECK_NEAR(increment, want, want * 1e-12);
            CHECK(x3pio_document_axis(x3pio_reader_document(reader), X3PIO_Z)
                      .storage
                  == stored);
            double *back = read_heights_of(reader);
            for (size_t i = 0; i < count; ++i)
            {
                if (i == 17)
                {
                    CHECK(isnan(back[i]));
                }
                else
                {
                    CHECK_NEAR(back[i], z[i], increment * 0.5 + 1e-18);
                }
            }
            /* The range of the integer is used. */
            int16_t largest = 0;
            if (type == 0)
            {
                double maximum = 0.0;
                for (size_t i = 0; i < count; ++i)
                {
                    maximum =
                        fmax(maximum,
                             isnan(back[i]) ? 0.0 : fabs(back[i] / increment));
                }
                largest = (int16_t)nearbyint(maximum);
                CHECK(largest >= 32766);
            }
            free(back);
            x3pio_reader_free(reader);
            x3pio_writer_free(writer);
            x3pio_document_free(doc);
            x3pio_buffer_free(&bytes);
        }
    }

    /* A file that stores x and y is written by points. */
    x3pio_document *doc = NULL;
    CHECK_OK(x3pio_document_create(&doc));
    CHECK_OK(x3pio_document_set_feature_irregular_surface(
        doc, columns, rows, X3PIO_TYPE_DOUBLE, 1.0, 1));
    x3pio_writer *writer = NULL;
    CHECK_OK(x3pio_writer_create(&writer));
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK_OK(create_in_buffer(writer, doc, &bytes));
    CHECK_STATUS(
        X3PIO_ERR_ARGUMENT,
        x3pio_write_heights(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
    CHECK(strstr(x3pio_writer_error_message(writer), "stores x and y") != NULL);
    x3pio_writer_free(writer);
    x3pio_document_free(doc);
    x3pio_buffer_free(&bytes);
    free(z);
}

/* An array of structures of the program, as a std::vector of points is, holds
 * every shape: the coordinates of a regular grid, an irregular one, layers, a
 * profile and a cloud. The rows come in pieces that continue in the array, the
 * other members stay as they were, and the array written to a file gives the
 * same points back. */
typedef struct vertex
{
    float x;
    float y;
    float z;
    unsigned char colour[3];
} vertex;

static bool same_float(float a, float b)
{
    return isnan(a) ? isnan(b) : a == b;
}

/* The description of the shape of a scenario, with the points stored as floats.
 */
static x3pio_document *float_document(const scenario *s)
{
    x3pio_document *doc = scenario_document(s, X3PIO_REVISION_AMD1);
    if (s->feature == X3PIO_POINT_CLOUD)
    {
        CHECK_OK(x3pio_document_set_feature_point_cloud(doc, s->columns,
                                                        X3PIO_TYPE_FLOAT, 1.0));
    }
    else if (!s->regular && s->feature == X3PIO_PROFILE)
    {
        CHECK_OK(x3pio_document_set_feature_irregular_profile(
            doc, s->columns, X3PIO_TYPE_FLOAT, 1.0, s->layers));
    }
    else if (!s->regular)
    {
        CHECK_OK(x3pio_document_set_feature_irregular_surface(
            doc, s->columns, s->rows, X3PIO_TYPE_FLOAT, 1.0, s->layers));
    }
    else if (s->feature == X3PIO_PROFILE)
    {
        CHECK_OK(x3pio_document_set_feature_profile(
            doc, s->columns, GRID_STEP, X3PIO_TYPE_FLOAT, 1.0, s->layers));
    }
    else
    {
        CHECK_OK(x3pio_document_set_feature_surface(
            doc, s->columns, s->rows, GRID_STEP, GRID_STEP, X3PIO_TYPE_FLOAT,
            1.0, s->layers));
    }
    return doc;
}

static void test_structures_of_every_shape(void)
{
    for (size_t k = 0; k < SCENARIO_COUNT; ++k)
    {
        const scenario *s = &scenarios[k];
        const size_t count = scenario_points(s);
        x3pio_buffer bytes;
        x3pio_buffer_init(&bytes, x3pio_default_allocator());
        x3pio_reader *reader =
            open_scenario(s, X3PIO_REVISION_AMD1, X3PIO_STORAGE_BINARY, &bytes);
        const x3pio_document *doc = x3pio_reader_document(reader);
        const size_t columns = x3pio_document_columns(doc);
        const size_t rows = x3pio_document_rows(doc);

        vertex *vertices = malloc(count * sizeof *vertices);
        for (size_t i = 0; i < count; ++i)
        {
            vertices[i] = (vertex){0, 0, 0, {1, 2, 3}};
        }
        x3pio_fields into;
        x3pio_fields_init(&into);
        const ptrdiff_t stride = sizeof(vertex);
        for (size_t row = 0; row < rows; row += 2)
        {
            const size_t piece = rows - row < 2 ? rows - row : 2;
            vertex *first = vertices + row * columns;
            into.x = (x3pio_field){&first->x, {X3PIO_TYPE_FLOAT, stride, 0}};
            into.y = (x3pio_field){&first->y, {X3PIO_TYPE_FLOAT, stride, 0}};
            into.z = (x3pio_field){&first->z, {X3PIO_TYPE_FLOAT, stride, 0}};
            CHECK_OK(x3pio_read_points(reader, &into, piece));
        }
        x3pio_reader_free(reader);

        double *x = malloc(count * sizeof *x);
        double *y = malloc(count * sizeof *y);
        double *z = malloc(count * sizeof *z);
        reader = NULL;
        CHECK_OK(x3pio_reader_create(&reader));
        CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
        read_metres(reader, x, y, z);
        x3pio_reader_free(reader);
        for (size_t i = 0; i < count; ++i)
        {
            CHECK(same_float(vertices[i].x, (float)x[i]));
            CHECK(same_float(vertices[i].y, (float)y[i]));
            CHECK(same_float(vertices[i].z, (float)z[i]));
            CHECK(vertices[i].colour[0] == 1 && vertices[i].colour[1] == 2
                  && vertices[i].colour[2] == 3);
        }

        /* The structures written to a file of floats come back as they were. */
        x3pio_document *copy = float_document(s);
        x3pio_writer *writer = NULL;
        CHECK_OK(x3pio_writer_create(&writer));
        x3pio_buffer second;
        x3pio_buffer_init(&second, x3pio_default_allocator());
        CHECK_OK(create_in_buffer(writer, copy, &second));
        x3pio_const_fields from;
        x3pio_const_fields_init(&from);
        from.z =
            (x3pio_const_field){&vertices->z, {X3PIO_TYPE_FLOAT, stride, 0}};
        /* A regular grid stores no x and y, and a profile no y. */
        if (scenario_stores_x(s))
        {
            from.x = (x3pio_const_field){&vertices->x,
                                         {X3PIO_TYPE_FLOAT, stride, 0}};
        }
        if (scenario_stores_y(s))
        {
            from.y = (x3pio_const_field){&vertices->y,
                                         {X3PIO_TYPE_FLOAT, stride, 0}};
        }
        CHECK_OK(x3pio_write_points(writer, &from, rows));
        CHECK_OK(x3pio_end_file(writer));
        CHECK_OK(x3pio_reader_create(&reader));
        CHECK_OK(x3pio_open_memory(reader, second.data, second.size));
        double *x2 = malloc(count * sizeof *x2);
        double *y2 = malloc(count * sizeof *y2);
        double *z2 = malloc(count * sizeof *z2);
        read_metres(reader, x2, y2, z2);
        for (size_t i = 0; i < count; ++i)
        {
            CHECK(same_float((float)x2[i], vertices[i].x));
            CHECK(same_float((float)y2[i], vertices[i].y));
            CHECK(same_float((float)z2[i], vertices[i].z));
        }
        x3pio_reader_free(reader);
        x3pio_writer_free(writer);
        x3pio_document_free(copy);
        x3pio_buffer_free(&second);
        x3pio_buffer_free(&bytes);
        free(x);
        free(y);
        free(z);
        free(x2);
        free(y2);
        free(z2);
        free(vertices);
    }
}

/* A list of points is a column of rows of one point: a cloud is read and
 * written in pieces of any number of points, whatever the chunks of the
 * library are, and the pieces give the same points as one call. */
static void test_point_lists_in_pieces(void)
{
    const char *names[] = {"point cloud float", "point cloud int16",
                           "point cloud int32", "point cloud double"};
    const x3pio_storage storages[] = {X3PIO_STORAGE_BINARY, X3PIO_STORAGE_TEXT};
    for (size_t n = 0; n < sizeof names / sizeof names[0]; ++n)
    {
        for (size_t k = 0; k < 2; ++k)
        {
            const scenario *s = find_scenario(names[n]);
            const size_t count = s->columns;
            x3pio_buffer bytes;
            x3pio_buffer_init(&bytes, x3pio_default_allocator());
            write_scenario(s, X3PIO_REVISION_AMD1, storages[k], &bytes);

            double *x = malloc(count * sizeof *x);
            double *y = malloc(count * sizeof *y);
            double *z = malloc(count * sizeof *z);
            x3pio_reader *reader = NULL;
            CHECK_OK(x3pio_reader_create(&reader));
            CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
            read_metres(reader, x, y, z);
            x3pio_reader_free(reader);

            /* Pieces of 7 points with chunks of 3: the pieces and the chunks
             * do not line up. */
            vertex *vertices = malloc(count * sizeof *vertices);
            CHECK_OK(x3pio_reader_create(&reader));
            CHECK_OK(x3pio_reader_set_chunk(reader, 3));
            CHECK_OK(x3pio_open_memory(reader, bytes.data, bytes.size));
            const x3pio_document *doc = x3pio_reader_document(reader);
            CHECK(x3pio_document_columns(doc) == 1);
            CHECK(x3pio_document_rows(doc) == count);
            const ptrdiff_t stride = sizeof(vertex);
            for (size_t first = 0; first < count; first += 7)
            {
                const size_t piece = count - first < 7 ? count - first : 7;
                x3pio_fields into;
                x3pio_fields_init(&into);
                into.x = (x3pio_field){&vertices[first].x,
                                       {X3PIO_TYPE_FLOAT, stride, 0}};
                into.y = (x3pio_field){&vertices[first].y,
                                       {X3PIO_TYPE_FLOAT, stride, 0}};
                into.z = (x3pio_field){&vertices[first].z,
                                       {X3PIO_TYPE_FLOAT, stride, 0}};
                CHECK_OK(x3pio_read_points(reader, &into, piece));
            }
            /* No point is left. */
            x3pio_fields more;
            x3pio_fields_init(&more);
            more.z =
                (x3pio_field){&vertices[0].z, {X3PIO_TYPE_FLOAT, stride, 0}};
            CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                         x3pio_read_points(reader, &more, 1));
            x3pio_reader_free(reader);
            for (size_t i = 0; i < count; ++i)
            {
                CHECK(vertices[i].x == (float)x[i]);
                CHECK(vertices[i].y == (float)y[i]);
                CHECK(vertices[i].z == (float)z[i]);
            }

            /* Written in pieces of 13 points, the file has the same points. */
            x3pio_document *doc2 = scenario_document(s, X3PIO_REVISION_AMD1);
            x3pio_writer *writer = NULL;
            CHECK_OK(x3pio_writer_create(&writer));
            CHECK_OK(x3pio_writer_set_chunk(writer, 5));
            CHECK_OK(x3pio_writer_set_storage(writer, storages[k]));
            x3pio_buffer second;
            x3pio_buffer_init(&second, x3pio_default_allocator());
            CHECK_OK(create_in_buffer(writer, doc2, &second));
            for (size_t first = 0; first < count; first += 13)
            {
                const size_t piece = count - first < 13 ? count - first : 13;
                x3pio_const_fields from;
                x3pio_const_fields_init(&from);
                from.x = (x3pio_const_field){
                    x + first, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
                from.y = (x3pio_const_field){
                    y + first, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
                from.z = (x3pio_const_field){
                    z + first, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
                CHECK_OK(x3pio_write_points(writer, &from, piece));
            }
            /* The file lacks no point and has room for none. */
            x3pio_const_fields extra;
            x3pio_const_fields_init(&extra);
            extra.z =
                (x3pio_const_field){z, *x3pio_layout_packed(X3PIO_TYPE_DOUBLE)};
            CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                         x3pio_write_points(writer, &extra, 1));
            CHECK_OK(x3pio_end_file(writer));
            x3pio_writer_free(writer);
            x3pio_document_free(doc2);

            double *x2 = malloc(count * sizeof *x2);
            double *y2 = malloc(count * sizeof *y2);
            double *z2 = malloc(count * sizeof *z2);
            CHECK_OK(x3pio_reader_create(&reader));
            CHECK_OK(x3pio_open_memory(reader, second.data, second.size));
            read_metres(reader, x2, y2, z2);
            x3pio_reader_free(reader);
            for (size_t i = 0; i < count; ++i)
            {
                CHECK(x2[i] == x[i] && y2[i] == y[i] && z2[i] == z[i]);
            }
            x3pio_buffer_free(&second);
            x3pio_buffer_free(&bytes);
            free(x);
            free(y);
            free(z);
            free(x2);
            free(y2);
            free(z2);
            free(vertices);
        }
    }
}

int main(void)
{
    test_every_shape();
    test_rewrite();
    test_image_layouts();
    test_structures();
    test_structures_of_every_shape();
    test_point_lists_in_pieces();
    test_layers();
    test_pieces();
    test_heights_and_automatic_increment();
    return finish_tests();
}
