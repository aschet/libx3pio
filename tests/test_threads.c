/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* Threads read and write different files, or read the same one, at the same
 * time, each with handles of its own. The library keeps no state of its own. */

#include "helpers.h"

#include <x3pio/file.h>

#include <locale.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>

#define THREADS 8
#define ROUNDS 12

/* The heights that thread n writes. */
static void heights_of(int n, double *z)
{
    for (size_t i = 0; i < 40 * 30; ++i)
    {
        z[i] = 1e-6 * sin(0.01 * (double)i * (double)(n + 1));
    }
    z[17 + (size_t)n] = NAN;
}

/* Builds the file of thread n. It checks nothing with the harness, which counts
 * in variables that the threads share. */
static bool build(int n, x3pio_storage storage, x3pio_buffer *out)
{
    static const x3pio_type types[4] = {X3PIO_TYPE_FLOAT, X3PIO_TYPE_DOUBLE,
                                        X3PIO_TYPE_INT16, X3PIO_TYPE_INT32};
    x3pio_document *doc = NULL;
    x3pio_writer *writer = NULL;
    double *z = malloc(40 * 30 * sizeof *z);
    const x3pio_type type = types[n % 4];
    const x3pio_timestamp date = {2026, 3, 4, 12, 30, n, 0, true, 60};
    bool ok = z != NULL && x3pio_document_create(&doc) == X3PIO_OK;
    ok = ok
         && x3pio_document_set_feature_surface(
                doc, 40, 30, 1e-6, 1e-6, type,
                type == X3PIO_TYPE_FLOAT || type == X3PIO_TYPE_DOUBLE
                    ? 1.0
                    : X3PIO_AUTO_INCREMENT,
                1)
                == X3PIO_OK
         && x3pio_document_set_date(doc, &date) == X3PIO_OK
         && x3pio_document_set_probing_type(doc, X3PIO_PROBING_SOFTWARE)
                == X3PIO_OK
         && x3pio_document_set_comment(doc, "decimal 1.5 and more") == X3PIO_OK;
    if (ok)
    {
        heights_of(n, z);
        ok = x3pio_writer_create(&writer) == X3PIO_OK
             && x3pio_writer_set_storage(writer, storage) == X3PIO_OK
             && create_in_buffer(writer, doc, out) == X3PIO_OK
             && x3pio_write_z(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z)
                    == X3PIO_OK
             && x3pio_end_file(writer) == X3PIO_OK;
    }
    x3pio_writer_free(writer);
    x3pio_document_free(doc);
    free(z);
    return ok;
}

static bool same(const x3pio_buffer *a, const x3pio_buffer *b)
{
    return a->size == b->size && memcmp(a->data, b->data, a->size) == 0;
}

typedef struct job
{
    const x3pio_buffer *expected; /* the file that the thread must produce */
    const char *path;
    int n;
    bool ok;
} job;

static void *write_and_read(void *argument)
{
    job *j = argument;
    j->ok = true;
    for (int round = 0; round < ROUNDS; ++round)
    {
        const x3pio_storage storage =
            round % 2 == 0 ? X3PIO_STORAGE_BINARY : X3PIO_STORAGE_TEXT;
        x3pio_buffer out;
        x3pio_buffer_init(&out, x3pio_default_allocator());
        if (!build(j->n, storage, &out))
        {
            j->ok = false;
        }
        if (storage == X3PIO_STORAGE_BINARY && !same(&out, j->expected))
        {
            j->ok = false;
        }
        x3pio_reader *reader = NULL;
        if (x3pio_reader_create(&reader) != X3PIO_OK
            || x3pio_open_memory(reader, out.data, out.size) != X3PIO_OK)
        {
            j->ok = false;
        }
        else
        {
            double *z = malloc(40 * 30 * sizeof *z);
            double *want = malloc(40 * 30 * sizeof *want);
            heights_of(j->n, want);
            if (x3pio_read_z(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z)
                != X3PIO_OK)
            {
                j->ok = false;
            }
            else
            {
                for (size_t i = 0; i < 40 * 30; ++i)
                {
                    if (isnan(want[i]) != isnan(z[i])
                        || (!isnan(want[i])
                            && fabs(want[i] - z[i]) > 1e-6 * 2e-4))
                    {
                        j->ok = false;
                    }
                }
            }
            free(z);
            free(want);
        }
        x3pio_reader_free(reader);
        /* A file of its own on disk. */
        x3pio_writer *writer = NULL;
        x3pio_document *doc = NULL;
        if (x3pio_writer_create(&writer) != X3PIO_OK
            || x3pio_document_create(&doc) != X3PIO_OK
            || x3pio_document_set_feature_surface(doc, 4, 3, 1e-6, 1e-6,
                                                  X3PIO_TYPE_FLOAT, 1.0, 1)
                   != X3PIO_OK
            || x3pio_create_file(writer, j->path, doc) != X3PIO_OK)
        {
            j->ok = false;
        }
        else
        {
            double z[12] = {0};
            if (x3pio_write_z(writer, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z)
                    != X3PIO_OK
                || x3pio_end_file(writer) != X3PIO_OK)
            {
                j->ok = false;
            }
        }
        x3pio_writer_free(writer);
        x3pio_document_free(doc);
        x3pio_reader *again = NULL;
        if (x3pio_reader_create(&again) != X3PIO_OK
            || x3pio_open_file(again, j->path) != X3PIO_OK)
        {
            j->ok = false;
        }
        x3pio_reader_free(again);
        remove(j->path);
        x3pio_buffer_free(&out);
    }
    return NULL;
}

static void test_different_files(void)
{
    x3pio_buffer expected[THREADS];
    job jobs[THREADS];
    char paths[THREADS][64];
    for (int n = 0; n < THREADS; ++n)
    {
        x3pio_buffer_init(&expected[n], x3pio_default_allocator());
        CHECK(build(n, X3PIO_STORAGE_BINARY, &expected[n]));
        snprintf(paths[n], sizeof paths[n], "test_threads_%d.x3p", n);
        jobs[n] = (job){&expected[n], paths[n], n, false};
    }
    pthread_t threads[THREADS];
    for (int n = 0; n < THREADS; ++n)
    {
        CHECK(pthread_create(&threads[n], NULL, write_and_read, &jobs[n]) == 0);
    }
    for (int n = 0; n < THREADS; ++n)
    {
        pthread_join(threads[n], NULL);
        CHECK(jobs[n].ok);
        x3pio_buffer_free(&expected[n]);
    }
}

typedef struct shared
{
    const x3pio_buffer *bytes;
    const char *path;
    bool from_file;
    bool ok;
} shared;

static void *read_shared(void *argument)
{
    shared *s = argument;
    s->ok = true;
    for (int round = 0; round < ROUNDS * 4; ++round)
    {
        x3pio_reader *reader = NULL;
        if (x3pio_reader_create(&reader) != X3PIO_OK)
        {
            s->ok = false;
            break;
        }
        const x3pio_status opened =
            s->from_file
                ? x3pio_open_file(reader, s->path)
                : x3pio_open_memory(reader, s->bytes->data, s->bytes->size);
        double *z = malloc(40 * 30 * sizeof *z);
        double *want = malloc(40 * 30 * sizeof *want);
        heights_of(1, want);
        if (opened != X3PIO_OK
            || x3pio_read_z(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z)
                   != X3PIO_OK)
        {
            s->ok = false;
        }
        else
        {
            for (size_t i = 0; i < 40 * 30; ++i)
            {
                if (isnan(want[i]) != isnan(z[i])
                    || (!isnan(want[i]) && fabs(want[i] - z[i]) > 1e-12))
                {
                    s->ok = false;
                }
            }
        }
        free(z);
        free(want);
        x3pio_reader_free(reader);
    }
    return NULL;
}

static void test_same_file(void)
{
    x3pio_buffer bytes;
    x3pio_buffer_init(&bytes, x3pio_default_allocator());
    CHECK(build(1, X3PIO_STORAGE_BINARY, &bytes));
    const char *path = "test_threads_shared.x3p";
    FILE *file = fopen(path, "wb");
    CHECK(file != NULL);
    if (file != NULL)
    {
        fwrite(bytes.data, 1, bytes.size, file);
        fclose(file);
    }
    shared jobs[THREADS];
    pthread_t threads[THREADS];
    for (int n = 0; n < THREADS; ++n)
    {
        jobs[n] = (shared){&bytes, path, n % 2 == 0, false};
        CHECK(pthread_create(&threads[n], NULL, read_shared, &jobs[n]) == 0);
    }
    for (int n = 0; n < THREADS; ++n)
    {
        pthread_join(threads[n], NULL);
        CHECK(jobs[n].ok);
    }
    remove(path);
    x3pio_buffer_free(&bytes);
}

static void *clock_and_text(void *argument)
{
    bool *ok = argument;
    *ok = true;
    for (int i = 0; i < 200; ++i)
    {
        x3pio_timestamp now;
        if (x3pio_timestamp_now(&now) != X3PIO_OK || now.year < 2026)
        {
            *ok = false;
        }
        char text[64];
        if (x3pio_timestamp_format(&now, text, sizeof text) == 0)
        {
            *ok = false;
        }
    }
    return NULL;
}

/* Text is read and written the same whatever the locale says. */
static void test_locale(void)
{
    x3pio_buffer in_c;
    x3pio_buffer_init(&in_c, x3pio_default_allocator());
    CHECK(build(2, X3PIO_STORAGE_TEXT, &in_c));

    const char *old = setlocale(LC_ALL, NULL);
    char saved[64];
    snprintf(saved, sizeof saved, "%s", old != NULL ? old : "C");
    const char *locales[] = {"de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8",
                             "de_DE", "de_AT.UTF-8"};
    bool set = false;
    for (size_t i = 0; i < 5 && !set; ++i)
    {
        set = setlocale(LC_ALL, locales[i]) != NULL;
    }
    if (set)
    {
        x3pio_buffer in_comma;
        x3pio_buffer_init(&in_comma, x3pio_default_allocator());
        CHECK(build(2, X3PIO_STORAGE_TEXT, &in_comma));
        CHECK(same(&in_c, &in_comma));
        x3pio_reader *reader = NULL;
        CHECK_OK(x3pio_reader_create(&reader));
        CHECK_OK(x3pio_open_memory(reader, in_c.data, in_c.size));
        double *z = malloc(40 * 30 * sizeof *z);
        double *want = malloc(40 * 30 * sizeof *want);
        heights_of(2, want);
        CHECK_OK(
            x3pio_read_z(reader, x3pio_layout_packed(X3PIO_TYPE_DOUBLE), z));
        for (size_t i = 0; i < 40 * 30; ++i)
        {
            CHECK(isnan(want[i]) == isnan(z[i]));
        }
        free(z);
        free(want);
        x3pio_reader_free(reader);
        x3pio_buffer_free(&in_comma);
        setlocale(LC_ALL, saved);
    }
    else
    {
        printf("no locale with a decimal comma: skipped\n");
    }
    x3pio_buffer_free(&in_c);

    bool ok[THREADS];
    pthread_t threads[THREADS];
    for (int n = 0; n < THREADS; ++n)
    {
        CHECK(pthread_create(&threads[n], NULL, clock_and_text, &ok[n]) == 0);
    }
    for (int n = 0; n < THREADS; ++n)
    {
        pthread_join(threads[n], NULL);
        CHECK(ok[n]);
    }
}

int main(void)
{
    test_different_files();
    test_same_file();
    test_locale();
    return finish_tests();
}
