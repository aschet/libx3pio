/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The description of a file: setters and getters, checks, copies. */

#include "check.h"
#include "document.h"

static x3pio_document *create(void)
{
    x3pio_document *d = NULL;
    CHECK_OK(x3pio_document_create(NULL, &d));
    return d;
}

static void test_defaults(void)
{
    x3pio_document *d = create();
    CHECK(x3pio_document_revision(d) == X3PIO_REVISION_AMD1);
    CHECK(x3pio_document_feature(d) == X3PIO_SURFACE);
    CHECK(x3pio_document_axis(d, X3PIO_X).type == X3PIO_AXIS_INCREMENTAL);
    CHECK(x3pio_document_axis(d, X3PIO_Z).type == X3PIO_AXIS_ABSOLUTE);
    CHECK(x3pio_document_axis(d, X3PIO_Z).storage == X3PIO_TYPE_DOUBLE);
    CHECK(x3pio_document_axis(d, X3PIO_Y).increment == 1.0);
    CHECK(!x3pio_document_has_metadata(d));
    CHECK_STRING(x3pio_document_creator(d), "");
    CHECK(x3pio_document_point_count(d) == 0);
    CHECK(x3pio_document_vendor_count(d) == 0);
    CHECK(x3pio_document_vendor_id(d, 0) == NULL);
    CHECK(!x3pio_placement_has_rotation(&d->placement));
    const char *reason = NULL;
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_validate(d, &reason));
    CHECK_STRING(reason, "The dimensions of the point data must not be zero");
    x3pio_document_free(d);
    x3pio_document_free(NULL);

    /* A null document gives neutral answers. */
    CHECK(x3pio_document_revision(NULL) == X3PIO_REVISION_AMD1);
    CHECK_STRING(x3pio_document_comment(NULL), "");
    CHECK(x3pio_document_point_count(NULL) == 0);
    CHECK(!x3pio_document_date(NULL, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_validate(NULL, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_create(NULL, NULL));
}

static void test_descriptions(void)
{
    x3pio_document *d = create();
    CHECK_OK(x3pio_document_set_surface(d, 640, 480, 1e-6, 2e-6,
                                        X3PIO_TYPE_FLOAT, 1.0, 1));
    CHECK(x3pio_document_feature(d) == X3PIO_SURFACE);
    CHECK(x3pio_document_axis(d, X3PIO_X).increment == 1e-6);
    CHECK(x3pio_document_axis(d, X3PIO_Y).increment == 2e-6);
    CHECK(x3pio_document_axis(d, X3PIO_Z).storage == X3PIO_TYPE_FLOAT);
    CHECK(x3pio_document_point_count(d) == 640 * 480);
    CHECK_OK(x3pio_document_validate(d, NULL));

    CHECK_OK(
        x3pio_document_set_profile(d, 256, 1e-6, X3PIO_TYPE_INT16, 1e-9, 3));
    CHECK(x3pio_document_feature(d) == X3PIO_PROFILE);
    x3pio_dimensions dims = x3pio_document_dimensions(d);
    CHECK(dims.size_x == 256 && dims.size_y == 1 && dims.size_z == 3
          && !dims.list);
    CHECK(x3pio_document_axis(d, X3PIO_Y).increment == 1e-6);
    CHECK_OK(x3pio_document_validate(d, NULL));

    CHECK_OK(x3pio_document_set_point_cloud(d, 1000, X3PIO_TYPE_INT32, 1e-8));
    CHECK(x3pio_document_feature(d) == X3PIO_POINT_CLOUD);
    dims = x3pio_document_dimensions(d);
    CHECK(dims.list && dims.size_x == 1000);
    for (int c = 0; c < 3; ++c)
    {
        const x3pio_axis axis = x3pio_document_axis(d, (x3pio_coordinate)c);
        CHECK(axis.type == X3PIO_AXIS_ABSOLUTE
              && axis.storage == X3PIO_TYPE_INT32 && axis.increment == 1e-8);
    }
    CHECK_OK(x3pio_document_validate(d, NULL));

    CHECK_OK(x3pio_document_set_irregular_surface(d, 96, 72, X3PIO_TYPE_DOUBLE,
                                                  1.0, 2));
    CHECK(x3pio_document_feature(d) == X3PIO_SURFACE);
    dims = x3pio_document_dimensions(d);
    CHECK(!dims.list && dims.size_x == 96 && dims.size_y == 72
          && dims.size_z == 2);
    CHECK(x3pio_document_axis(d, X3PIO_X).type == X3PIO_AXIS_ABSOLUTE);
    CHECK_OK(x3pio_document_validate(d, NULL));

    /* A bad number changes nothing. */
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_surface(d, 4, 4, 0.0, 1.0, X3PIO_TYPE_FLOAT,
                                            1.0, 1));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_surface(d, 4, 4, 1.0, NAN, X3PIO_TYPE_FLOAT,
                                            1.0, 1));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_surface(d, 4, 4, 1.0, 1.0, X3PIO_TYPE_UINT8,
                                            1.0, 1));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_surface(d, 4, 4, 1.0, 1.0, X3PIO_TYPE_FLOAT,
                                            -1.0, 1));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_point_cloud(d, 4, X3PIO_TYPE_INT8, 1.0));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_surface(NULL, 4, 4, 1.0, 1.0,
                                            X3PIO_TYPE_FLOAT, 1.0, 1));
    CHECK(x3pio_document_dimensions(d).size_x == 96);
    x3pio_document_free(d);
}

static void test_rows_and_automatic_increment(void)
{
    x3pio_document *d = create();
    CHECK_OK(x3pio_document_set_surface(
        d, 640, 480, 1e-6, 1e-6, X3PIO_TYPE_INT16, X3PIO_AUTO_INCREMENT, 3));
    CHECK(x3pio_document_columns(d) == 640);
    CHECK(x3pio_document_rows(d) == 480 * 3);
    CHECK_OK(x3pio_document_validate(d, NULL));
    CHECK(x3pio_document_axis(d, X3PIO_Z).increment == X3PIO_AUTO_INCREMENT);
    /* Floats store the metres: there is nothing to select. */
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_set_surface(
                                         d, 4, 4, 1e-6, 1e-6, X3PIO_TYPE_FLOAT,
                                         X3PIO_AUTO_INCREMENT, 1));
    x3pio_axis axis = {X3PIO_AXIS_ABSOLUTE, X3PIO_TYPE_FLOAT,
                       X3PIO_AUTO_INCREMENT};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_axis(d, X3PIO_Z, &axis));
    /* Only the z axis can leave the increment to the writer. */
    axis = (x3pio_axis){X3PIO_AXIS_INCREMENTAL, X3PIO_TYPE_INT16,
                        X3PIO_AUTO_INCREMENT};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_axis(d, X3PIO_X, &axis));
    axis = (x3pio_axis){X3PIO_AXIS_ABSOLUTE, X3PIO_TYPE_INT32,
                        X3PIO_AUTO_INCREMENT};
    CHECK_OK(x3pio_document_set_axis(d, X3PIO_Z, &axis));

    /* A list is a column of rows of one point each. */
    CHECK_OK(x3pio_document_set_point_cloud(d, 1000, X3PIO_TYPE_FLOAT, 1.0));
    CHECK(x3pio_document_columns(d) == 1);
    CHECK(x3pio_document_rows(d) == 1000);
    CHECK_OK(x3pio_document_set_matrix(d, 4, SIZE_MAX / 2, 4));
    CHECK(x3pio_document_rows(d) == 0);
    CHECK(x3pio_document_columns(NULL) == 0 && x3pio_document_rows(NULL) == 0);
    CHECK(!x3pio_document_has_validity(d));
    CHECK(!x3pio_document_has_validity(NULL));
    x3pio_document_free(d);
}

static void test_validate(void)
{
    x3pio_document *d = create();
    const char *reason = "";
    CHECK_OK(x3pio_document_set_surface(d, 4, 3, 1.0, 1.0, X3PIO_TYPE_DOUBLE,
                                        1.0, 1));
    CHECK_OK(x3pio_document_validate(d, &reason));
    CHECK_STRING(reason, "");

    x3pio_axis axis = {X3PIO_AXIS_INCREMENTAL, X3PIO_TYPE_DOUBLE, 1.0};
    CHECK_OK(x3pio_document_set_axis(d, X3PIO_Z, &axis));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_validate(d, &reason));
    CHECK_STRING(reason, "The z axis must be absolute");
    axis.type = X3PIO_AXIS_ABSOLUTE;
    CHECK_OK(x3pio_document_set_axis(d, X3PIO_Z, &axis));

    CHECK_OK(x3pio_document_set_feature(d, X3PIO_PROFILE));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_validate(d, &reason));
    CHECK(strstr(reason, "profile") != NULL);
    CHECK_OK(x3pio_document_set_feature(d, X3PIO_POINT_CLOUD));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_validate(d, &reason));
    CHECK(strstr(reason, "list") != NULL);
    CHECK_OK(x3pio_document_set_point_list(d, 5));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_validate(d, &reason));
    CHECK(strstr(reason, "absolute") != NULL);
    CHECK_OK(x3pio_document_set_feature(d, X3PIO_SURFACE));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_validate(d, &reason));
    CHECK_OK(x3pio_document_set_surface(d, 4, 3, 1.0, 1.0, X3PIO_TYPE_DOUBLE,
                                        1.0, 1));
    CHECK_OK(x3pio_document_set_matrix(d, 4, 0, 1));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_validate(d, &reason));
    CHECK_OK(x3pio_document_set_matrix(d, SIZE_MAX / 2, 4, 4));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_validate(d, &reason));
    CHECK(strstr(reason, "too large") != NULL);
    CHECK_OK(x3pio_document_set_matrix(d, 4, 3, 1));

    /* Metadata needs a date and the kind of probing system. */
    CHECK_OK(x3pio_document_set_comment(d, "note"));
    CHECK(x3pio_document_has_metadata(d));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_validate(d, &reason));
    CHECK_STRING(reason, "The metadata needs a date");
    x3pio_timestamp date = {2026, 1, 2, 3, 4, 5, 0, false, 0};
    CHECK_OK(x3pio_document_set_date(d, &date));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_validate(d, &reason));
    CHECK(strstr(reason, "probing") != NULL);
    CHECK_OK(x3pio_document_set_probing_type(d, X3PIO_PROBING_SOFTWARE));
    CHECK_OK(x3pio_document_validate(d, &reason));
    x3pio_document_clear_metadata(d);
    CHECK(!x3pio_document_has_metadata(d));
    CHECK_OK(x3pio_document_validate(d, &reason));
    x3pio_document_free(d);
}

static void test_setters(void)
{
    x3pio_document *d = create();
    x3pio_axis axis = {X3PIO_AXIS_ABSOLUTE, X3PIO_TYPE_UINT8, 1.0};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_axis(d, X3PIO_X, &axis));
    axis = (x3pio_axis){X3PIO_AXIS_ABSOLUTE, X3PIO_TYPE_INT16, 0.0};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_axis(d, X3PIO_X, &axis));
    axis.increment = INFINITY;
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_axis(d, X3PIO_X, &axis));
    axis.increment = 1e-3;
    CHECK_OK(x3pio_document_set_axis(d, X3PIO_X, &axis));
    CHECK(x3pio_document_axis(d, X3PIO_X).storage == X3PIO_TYPE_INT16);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_axis(d, (x3pio_coordinate)3, &axis));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_set_axis(d, X3PIO_X, NULL));
    /* An incremental axis does not store, so its type does not matter. */
    axis = (x3pio_axis){X3PIO_AXIS_INCREMENTAL, X3PIO_TYPE_UINT8, 2.0};
    CHECK_OK(x3pio_document_set_axis(d, X3PIO_Y, &axis));
    CHECK(x3pio_document_axis(d, X3PIO_Y).storage == X3PIO_TYPE_DOUBLE);

    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_revision(d, (x3pio_revision)7));
    CHECK_OK(x3pio_document_set_revision(d, X3PIO_REVISION_2017));
    CHECK(x3pio_document_revision(d) == X3PIO_REVISION_2017);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_feature(d, (x3pio_feature)9));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_probing_type(d, (x3pio_probing)9));

    x3pio_placement place = x3pio_placement_identity();
    place.offset[1] = 0.5;
    CHECK_OK(x3pio_document_set_placement(d, &place));
    place.rotation[0] = 2.0;
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_set_placement(d, &place));
    place = x3pio_placement_identity();
    place.rotation[0] = -1.0; /* a mirror */
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_set_placement(d, &place));
    place = x3pio_placement_identity();
    place.rotation[1] = 0.1; /* not orthonormal */
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_set_placement(d, &place));
    place = x3pio_placement_identity();
    place.offset[2] = NAN;
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_set_placement(d, &place));
    CHECK(x3pio_document_placement(d).offset[1] == 0.5);

    x3pio_timestamp date = {2026, 2, 30, 0, 0, 0, 0, false, 0};
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_set_date(d, &date));
    CHECK(!x3pio_document_date(d, NULL));
    date.day = 28;
    CHECK_OK(x3pio_document_set_date(d, &date));
    x3pio_timestamp back;
    CHECK(x3pio_document_date(d, &back) && back.day == 28);
    CHECK_OK(x3pio_document_set_date(d, NULL));
    CHECK(!x3pio_document_date(d, NULL));
    CHECK_OK(x3pio_document_set_calibration_date(d, &date));
    CHECK(x3pio_document_calibration_date(d, &back) && back.month == 2);
    CHECK_OK(x3pio_document_set_calibration_date(d, NULL));
    CHECK(!x3pio_document_calibration_date(d, NULL));

    /* Texts are copied, replaced and read back. */
    char text[] = "first";
    CHECK_OK(x3pio_document_set_creator(d, text));
    text[0] = 'X';
    CHECK_STRING(x3pio_document_creator(d), "first");
    CHECK_OK(x3pio_document_set_creator(d, "second value"));
    CHECK_STRING(x3pio_document_creator(d), "second value");
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_set_creator(d, NULL));
    CHECK_OK(x3pio_document_set_manufacturer(d, "Acme"));
    CHECK_OK(x3pio_document_set_model(d, "M"));
    CHECK_OK(x3pio_document_set_serial(d, "S"));
    CHECK_OK(x3pio_document_set_version(d, "V"));
    CHECK_OK(x3pio_document_set_probing_identification(d, "P"));
    CHECK_OK(x3pio_document_set_comment(d, "C"));
    CHECK_STRING(x3pio_document_manufacturer(d), "Acme");
    CHECK_STRING(x3pio_document_model(d), "M");
    CHECK_STRING(x3pio_document_serial(d), "S");
    CHECK_STRING(x3pio_document_version(d), "V");
    CHECK_STRING(x3pio_document_probing_identification(d), "P");
    CHECK_STRING(x3pio_document_comment(d), "C");
    x3pio_document_free(d);
}

static void test_software(void)
{
    x3pio_document *d = create();
    CHECK_OK(x3pio_document_set_software_metadata(d, "my tool", "2.1"));
    CHECK(x3pio_document_has_metadata(d));
    CHECK_STRING(x3pio_document_model(d), "my tool");
    CHECK_STRING(x3pio_document_version(d), "2.1");
    CHECK(x3pio_document_probing_type(d) == X3PIO_PROBING_SOFTWARE);
    CHECK_STRING(x3pio_document_probing_identification(d), "my tool");
    x3pio_timestamp date;
    CHECK(x3pio_document_date(d, &date) && date.year >= 2026
          && date.has_offset);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_set_software_metadata(d, NULL, "1"));
    x3pio_document_free(d);
}

static void test_copy(void)
{
    x3pio_document *a = create();
    CHECK_OK(x3pio_document_set_software_metadata(a, "tool", "1"));
    CHECK_OK(x3pio_document_set_creator(a, "me"));
    CHECK_OK(x3pio_document_set_point_cloud(a, 7, X3PIO_TYPE_INT16, 1e-6));
    x3pio_placement place = x3pio_placement_identity();
    place.offset[0] = 3.0;
    CHECK_OK(x3pio_document_set_placement(a, &place));
    x3pio_document *b = create();
    CHECK_OK(x3pio_document_set_comment(b, "to be replaced"));
    CHECK_OK(x3pio_document_copy(b, a));
    CHECK(x3pio_document_feature(b) == X3PIO_POINT_CLOUD);
    CHECK(x3pio_document_dimensions(b).size_x == 7);
    CHECK_STRING(x3pio_document_creator(b), "me");
    CHECK_STRING(x3pio_document_comment(b), "");
    CHECK_STRING(x3pio_document_model(b), "tool");
    CHECK(x3pio_document_placement(b).offset[0] == 3.0);
    CHECK(x3pio_document_axis(b, X3PIO_X).storage == X3PIO_TYPE_INT16);
    /* The copy is independent. */
    CHECK_OK(x3pio_document_set_creator(a, "someone else"));
    CHECK_STRING(x3pio_document_creator(b), "me");
    CHECK_OK(x3pio_document_copy(a, a));
    CHECK_STRING(x3pio_document_creator(a), "someone else");
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_copy(NULL, a));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_copy(a, NULL));
    x3pio_document_free(a);
    x3pio_document_free(b);
}

/* The functions that return a structure through a pointer give what the ones
 * that return it by value give, for a language that cannot take a structure
 * as a return value. */
static void test_pointer_getters(void)
{
    x3pio_document *d = NULL;
    CHECK_OK(x3pio_document_create(NULL, &d));
    CHECK_OK(x3pio_document_set_surface(d, 7, 5, 2e-6, 3e-6, X3PIO_TYPE_INT16,
                                        1e-9, 2));
    x3pio_placement place = x3pio_placement_identity();
    place.offset[0] = 0.25;
    CHECK_OK(x3pio_document_set_placement(d, &place));

    for (int k = 0; k < 3; ++k)
    {
        x3pio_axis by_pointer;
        CHECK_OK(x3pio_document_get_axis(d, (x3pio_coordinate)k, &by_pointer));
        const x3pio_axis by_value = x3pio_document_axis(d, (x3pio_coordinate)k);
        CHECK(by_pointer.type == by_value.type
              && by_pointer.storage == by_value.storage
              && by_pointer.increment == by_value.increment);
    }
    x3pio_axis axis;
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_get_axis(d, X3PIO_X, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_get_axis(NULL, X3PIO_X, &axis));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_get_axis(d, (x3pio_coordinate)3, &axis));

    x3pio_placement got;
    CHECK_OK(x3pio_document_get_placement(d, &got));
    CHECK(got.offset[0] == 0.25 && got.rotation[0] == 1.0);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_get_placement(d, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_get_placement(NULL, &got));

    x3pio_dimensions dims;
    CHECK_OK(x3pio_document_get_dimensions(d, &dims));
    CHECK(dims.size_x == 7 && dims.size_y == 5 && dims.size_z == 2
          && !dims.list);
    CHECK_STATUS(X3PIO_ERR_ARGUMENT, x3pio_document_get_dimensions(d, NULL));
    CHECK_STATUS(X3PIO_ERR_ARGUMENT,
                 x3pio_document_get_dimensions(NULL, &dims));
    x3pio_document_free(d);

    /* Initialization of the structures in place. */
    x3pio_placement identity;
    x3pio_placement_init(&identity);
    const x3pio_placement expected = x3pio_placement_identity();
    for (size_t i = 0; i < 9; ++i)
    {
        CHECK(identity.rotation[i] == expected.rotation[i]);
    }
    for (size_t i = 0; i < 3; ++i)
    {
        CHECK(identity.offset[i] == expected.offset[i]);
    }
    x3pio_placement_init(NULL);
    x3pio_compression compression = {X3PIO_COMPRESS_STORE, 9};
    x3pio_compression_init(&compression);
    const x3pio_compression by_value = x3pio_compression_default();
    CHECK(compression.method == by_value.method
          && compression.level == by_value.level);
    x3pio_compression_init(NULL);
}

int main(void)
{
    test_defaults();
    test_descriptions();
    test_rows_and_automatic_increment();
    test_validate();
    test_setters();
    test_software();
    test_copy();
    test_pointer_getters();
    return finish_tests();
}
