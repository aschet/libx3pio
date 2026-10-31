/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The delivery of the Datum elements of main.xml, a part at a time: it finds
 * the Datum elements as the first parse of the document does, wherever the
 * bytes are cut, and refuses what the first parse refuses. */

#include "check.h"
#include "xml.h"

#include <x3pio/types.h>

#include <stdio.h>
#include <string.h>

/* The bytes of a text, handed out in pieces of `piece` bytes at most. */
typedef struct supplier
{
    const char *text;
    size_t size;
    size_t at;
    size_t piece;
} supplier;

static x3pio_status supply(void *user, void *buffer, size_t capacity,
                           size_t *got, const char **message)
{
    (void)message;
    supplier *s = user;
    size_t n = s->size - s->at;
    n = n < capacity ? n : capacity;
    n = n < s->piece ? n : s->piece;
    memcpy(buffer, s->text + s->at, n);
    s->at += n;
    *got = n;
    return X3PIO_OK;
}

static x3pio_status open_datums(supplier *s, const char *text, size_t piece,
                                size_t absolute, x3pio_xml_datums **datums)
{
    *s = (supplier){text, strlen(text), 0, piece};
    const char *message = "";
    return x3pio_xml_datums_open(x3pio_default_allocator(), supply, s, absolute,
                                 datums, &message);
}

static const char *const head =
    "<?xml version=\"1.0\"?><ISO5436_2 xmlns=\"http://www.opengps.eu/2008/"
    "ISO5436_2\"><Record1><Revision>ISO5436 - 2000</Revision></Record1>"
    "<Record2><Comment>x</Comment></Record2><Record3><ListDimension>5"
    "</ListDimension><DataList>";
static const char *const tail = "</DataList></Record3></ISO5436_2>";

static void test_values_in_pieces(void)
{
    char text[1024];
    snprintf(text, sizeof text,
             "%s<Datum>1;2;3</Datum><Datum> 4.5 ; 5 ; 6 </Datum><Datum/>"
             "<Datum>7e-1;8;9</Datum><Datum>1;1;1</Datum>%s",
             head, tail);
    /* Whatever the sizes of the pieces of the bytes and of the points. */
    for (size_t piece = 1; piece <= 64; piece += 7)
    {
        for (size_t first = 1; first <= 5; ++first)
        {
            supplier s;
            x3pio_xml_datums *datums = NULL;
            CHECK_OK(open_datums(&s, text, piece, 3, &datums));
            double values[15];
            unsigned char valid[5];
            double all[15];
            unsigned char all_valid[5];
            const char *message = "";
            CHECK_OK(
                x3pio_xml_datums_read(datums, first, values, valid, &message));
            memcpy(all, values, first * 3 * sizeof *all);
            memcpy(all_valid, valid, first);
            CHECK_OK(x3pio_xml_datums_read(datums, 5 - first, values, valid,
                                           &message));
            memcpy(all + first * 3, values, (5 - first) * 3 * sizeof *all);
            memcpy(all_valid + first, valid, 5 - first);
            const double expected[15] = {1,   2,   3, 4.5, 5, 6, NAN, NAN,
                                         NAN, 0.7, 8, 9,   1, 1, 1};
            const unsigned char expected_valid[5] = {1, 1, 0, 1, 1};
            for (size_t i = 0; i < 15; ++i)
            {
                CHECK(isnan(expected[i]) ? isnan(all[i])
                                         : all[i] == expected[i]);
            }
            CHECK(memcmp(all_valid, expected_valid, 5) == 0);
            x3pio_xml_datums_free(datums);
        }
    }
}

static void test_refused(void)
{
    char text[512];
    const char *message = "";
    double values[15];
    unsigned char valid[5];

    /* Fewer Datum elements than asked for. */
    snprintf(text, sizeof text, "%s<Datum>1;2;3</Datum>%s", head, tail);
    supplier s;
    x3pio_xml_datums *datums = NULL;
    CHECK_OK(open_datums(&s, text, 1000, 3, &datums));
    CHECK_OK(x3pio_xml_datums_read(datums, 1, values, valid, &message));
    CHECK_STATUS(X3PIO_ERR_FORMAT,
                 x3pio_xml_datums_read(datums, 1, values, valid, &message));
    CHECK_STRING(message, "DataList holds fewer Datum elements than points");
    x3pio_xml_datums_free(datums);

    /* A Datum with another number of values than the axes that are stored. */
    snprintf(text, sizeof text, "%s<Datum>1;2</Datum>%s", head, tail);
    CHECK_OK(open_datums(&s, text, 1000, 3, &datums));
    CHECK_STATUS(X3PIO_ERR_FORMAT,
                 x3pio_xml_datums_read(datums, 1, values, valid, &message));
    x3pio_xml_datums_free(datums);

    /* Text that is no number, and too many numbers. */
    snprintf(text, sizeof text, "%s<Datum>1;two;3</Datum>%s", head, tail);
    CHECK_OK(open_datums(&s, text, 1000, 3, &datums));
    CHECK_STATUS(X3PIO_ERR_FORMAT,
                 x3pio_xml_datums_read(datums, 1, values, valid, &message));
    CHECK_STRING(message, "A Datum is not a list of at most three numbers");
    x3pio_xml_datums_free(datums);
    snprintf(text, sizeof text, "%s<Datum>1;2;3;4</Datum>%s", head, tail);
    CHECK_OK(open_datums(&s, text, 1000, 3, &datums));
    CHECK_STATUS(X3PIO_ERR_FORMAT,
                 x3pio_xml_datums_read(datums, 1, values, valid, &message));
    x3pio_xml_datums_free(datums);

    /* A Datum that is longer than any list of three numbers. */
    char *huge = malloc(5000);
    memset(huge, '1', 4999);
    huge[4999] = '\0';
    char *long_text = malloc(6000);
    snprintf(long_text, 6000, "%s<Datum>%s</Datum>%s", head, huge, tail);
    CHECK_OK(open_datums(&s, long_text, 1000, 3, &datums));
    CHECK_STATUS(X3PIO_ERR_FORMAT,
                 x3pio_xml_datums_read(datums, 1, values, valid, &message));
    x3pio_xml_datums_free(datums);
    free(long_text);
    free(huge);

    /* Text that is not well-formed, found while the points are read. */
    snprintf(text, sizeof text, "%s<Datum>1;2;3</Datum></Wrong>", head);
    CHECK_OK(open_datums(&s, text, 1000, 3, &datums));
    CHECK_STATUS(X3PIO_ERR_FORMAT,
                 x3pio_xml_datums_read(datums, 2, values, valid, &message));
    x3pio_xml_datums_free(datums);
}

/* The Datum elements that count are those of the first DataList of Record3,
 * with any prefix on the root and with the records in any order. */
static void test_structure(void)
{
    const char *text =
        "<x:ISO5436_2 xmlns:x=\"urn:x\"><Record3><DataList><Datum>1;2;3</Datum>"
        "<Datum>4;5;6</Datum></DataList><DataList><Datum>9;9;9</Datum>"
        "</DataList></Record3><Record2><DataList><Datum>8;8;8</Datum>"
        "</DataList></Record2><Record1/></x:ISO5436_2>";
    supplier s;
    x3pio_xml_datums *datums = NULL;
    CHECK_OK(open_datums(&s, text, 5, 3, &datums));
    double values[6];
    unsigned char valid[2];
    const char *message = "";
    CHECK_OK(x3pio_xml_datums_read(datums, 2, values, valid, &message));
    CHECK(values[0] == 1 && values[3] == 4 && valid[0] == 1 && valid[1] == 1);
    /* Elements after the last point asked for are not read. */
    x3pio_xml_datums_free(datums);

    /* A file without a Datum can be opened: it holds no points. */
    CHECK_OK(open_datums(&s, "<ISO5436_2/>", 3, 3, &datums));
    CHECK_STATUS(X3PIO_ERR_FORMAT,
                 x3pio_xml_datums_read(datums, 1, values, valid, &message));
    x3pio_xml_datums_free(datums);
    /* No text at all is not XML, which the opening finds. */
    CHECK_STATUS(X3PIO_ERR_FORMAT, open_datums(&s, "", 3, 3, &datums));
    x3pio_xml_datums_free(NULL);
}

/* More than one chunk of the parser, and the Datum elements cut at its end. */
static void test_many(void)
{
    enum
    {
        COUNT = 20000
    };
    size_t capacity = strlen(head) + strlen(tail) + COUNT * 40;
    char *text = malloc(capacity);
    size_t length = (size_t)snprintf(text, capacity, "%s", head);
    for (size_t i = 0; i < COUNT; ++i)
    {
        length +=
            (size_t)snprintf(text + length, capacity - length,
                             "<Datum>%zu;%zu;%zu</Datum>", i, 2 * i, 3 * i);
    }
    snprintf(text + length, capacity - length, "%s", tail);
    supplier s;
    x3pio_xml_datums *datums = NULL;
    CHECK_OK(open_datums(&s, text, 1U << 20U, 3, &datums));
    static double values[1000 * 3];
    static unsigned char valid[1000];
    const char *message = "";
    size_t done = 0;
    size_t piece = 1;
    while (done < COUNT)
    {
        const size_t n = COUNT - done < piece ? COUNT - done : piece;
        CHECK_OK(x3pio_xml_datums_read(datums, n, values, valid, &message));
        for (size_t i = 0; i < n; ++i)
        {
            CHECK(values[3 * i] == (double)(done + i)
                  && values[3 * i + 2] == (double)(3 * (done + i))
                  && valid[i] == 1);
        }
        done += n;
        piece = piece * 7 % 997 + 1;
    }
    x3pio_xml_datums_free(datums);
    free(text);
}

int main(void)
{
    test_values_in_pieces();
    test_refused();
    test_structure();
    test_many();
    return finish_tests();
}
