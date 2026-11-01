/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Writing of main.xml.
 *
 * The text is written directly: the form of main.xml is fixed, and the schema
 * of the standard decides what a valid file is, so the writer needs no library.
 * What it writes is valid against the schema of the revision of the document.
 */

#include "allocator.h"
#include "text.h"
#include "timestamp.h"
#include "vendor.h"
#include "xml.h"
#include <math.h>

#define TARGET_NAMESPACE "http://www.opengps.eu/2008/ISO5436_2"
/* The text is passed on in pieces of about this size. */
#define EMIT_SIZE 32768

typedef struct out
{
    x3pio_buffer buffer;
    x3pio_xml_emit emit;
    void *user;
    const char **message;
    x3pio_status status;
    int depth;
    x3pio_decimal_point point;
} out;

static void out_fail(out *o, x3pio_status status, const char *text)
{
    if (o->status == X3PIO_OK)
    {
        o->status = x3pio_fail(o->message, status, text);
    }
}

static void out_flush(out *o)
{
    if (o->status == X3PIO_OK && o->buffer.size > 0)
    {
        o->status =
            o->emit(o->user, o->buffer.data, o->buffer.size, o->message);
    }
    o->buffer.size = 0;
}

static void put(out *o, const char *text, size_t size)
{
    if (o->status != X3PIO_OK)
    {
        return;
    }
    if (!x3pio_buffer_append(&o->buffer, text, size))
    {
        out_fail(o, X3PIO_ERR_MEMORY, "Out of memory");
        return;
    }
    if (o->buffer.size >= EMIT_SIZE)
    {
        out_flush(o);
    }
}

static void put_text(out *o, const char *text)
{
    put(o, text, strlen(text));
}

static void line(out *o)
{
    if (o->buffer.size > 0 || o->depth >= 0)
    {
        put(o, "\n", 1);
    }
    for (int i = 0; i < o->depth; ++i)
    {
        put(o, "  ", 2);
    }
}

static void open_element(out *o, const char *name)
{
    line(o);
    put(o, "<", 1);
    put_text(o, name);
    put(o, ">", 1);
    ++o->depth;
}

static void close_element(out *o, const char *name)
{
    --o->depth;
    line(o);
    put(o, "</", 2);
    put_text(o, name);
    put(o, ">", 1);
}

/* Writes a text, checked for what XML cannot hold and escaped. */
static void put_escaped(out *o, const char *text, size_t size)
{
    if (!x3pio_utf8_valid(text, size))
    {
        out_fail(o, X3PIO_ERR_ARGUMENT,
                 "A text of the metadata is not valid UTF-8");
        return;
    }
    size_t from = 0;
    for (size_t i = 0; i < size; ++i)
    {
        const unsigned char c = (unsigned char)text[i];
        const char *replacement = NULL;
        if (c < 0x20 && c != '\t' && c != '\n' && c != '\r')
        {
            out_fail(o, X3PIO_ERR_ARGUMENT,
                     "A text of the metadata holds a control character that "
                     "XML cannot hold");
            return;
        }
        switch (c)
        {
        case '&':
            replacement = "&amp;";
            break;
        case '<':
            replacement = "&lt;";
            break;
        case '>':
            replacement = "&gt;";
            break;
        case '\r':
            replacement = "&#13;";
            break;
        default:
            break;
        }
        if (replacement != NULL)
        {
            put(o, text + from, i - from);
            put_text(o, replacement);
            from = i + 1;
        }
    }
    put(o, text + from, size - from);
}

static void leaf(out *o, const char *name, const char *value, size_t size)
{
    line(o);
    put(o, "<", 1);
    put_text(o, name);
    put(o, ">", 1);
    put_escaped(o, value, size);
    put(o, "</", 2);
    put_text(o, name);
    put(o, ">", 1);
}

static void leaf_text(out *o, const char *name, const char *value)
{
    leaf(o, name, value, strlen(value));
}

/* A number of the document: the shortest text that reads back exactly. */
static void leaf_number(out *o, const char *name, double value)
{
    char text[X3PIO_NUMBER_CAPACITY];
    const size_t size =
        x3pio_format_number(&o->point, value, text, sizeof text);
    leaf(o, name, text, size);
}

/* xsd:token: white space is collapsed and trimmed. The result is in a buffer of
 * the caller, which it releases. */
static char *token(out *o, const x3pio_allocator *allocator, const char *text,
                   size_t *size)
{
    const size_t length = text != NULL ? strlen(text) : 0;
    char *result = x3pio_allocate(allocator, length + 1);
    if (result == NULL)
    {
        out_fail(o, X3PIO_ERR_MEMORY, "Out of memory");
        *size = 0;
        return NULL;
    }
    size_t used = 0;
    bool pending_space = false;
    for (size_t i = 0; i < length; ++i)
    {
        if (x3pio_is_space(text[i]))
        {
            pending_space = used > 0;
            continue;
        }
        if (pending_space)
        {
            result[used++] = ' ';
            pending_space = false;
        }
        result[used++] = text[i];
    }
    result[used] = '\0';
    *size = used;
    return result;
}

static void token_leaf(out *o, const x3pio_allocator *allocator,
                       const char *name, const char *text, bool skip_empty)
{
    size_t size = 0;
    char *value = token(o, allocator, text, &size);
    if (value != NULL && (size > 0 || !skip_empty))
    {
        leaf(o, name, value, size);
    }
    x3pio_release(allocator, value);
}

static void timestamp_leaf(out *o, const char *name, const x3pio_timestamp *t)
{
    char text[64];
    const size_t size = x3pio_timestamp_format(t, text, sizeof text);
    leaf(o, name, text, size < sizeof text ? size : 0);
}

static char type_code(x3pio_type type)
{
    switch (type)
    {
    case X3PIO_TYPE_INT16:
        return 'I';
    case X3PIO_TYPE_INT32:
        return 'L';
    case X3PIO_TYPE_FLOAT:
        return 'F';
    default:
        break;
    }
    return 'D';
}

static const char *probing_text(x3pio_probing probing)
{
    switch (probing)
    {
    case X3PIO_PROBING_CONTACTING:
        return "Contacting";
    case X3PIO_PROBING_NON_CONTACTING:
        return "NonContacting";
    case X3PIO_PROBING_SOFTWARE:
    case X3PIO_PROBING_UNKNOWN:
        break;
    }
    return "Software";
}

static const char *feature_text(x3pio_feature feature)
{
    switch (feature)
    {
    case X3PIO_PROFILE:
        return "PRF";
    case X3PIO_SURFACE:
        return "SUR";
    case X3PIO_POINT_CLOUD:
        break;
    }
    return "PCL";
}

static void write_metadata(out *o, const x3pio_document *d)
{
    if (!d->has_date || d->probing == X3PIO_PROBING_UNKNOWN)
    {
        out_fail(o, X3PIO_ERR_ARGUMENT,
                 "The metadata needs a date and the type of the probing "
                 "system");
        return;
    }
    const x3pio_allocator *a = &d->allocator;
    const bool amd1 = d->revision == X3PIO_REVISION_AMD1;
    open_element(o, "Record2");
    timestamp_leaf(o, "Date", &d->date);
    token_leaf(o, a, "Creator", d->creator, true);
    open_element(o, "Instrument");
    token_leaf(o, a, "Manufacturer", d->manufacturer, false);
    token_leaf(o, a, "Model", d->model, false);
    token_leaf(o, a, "Serial", d->serial, false);
    token_leaf(o, a, "Version", d->version, false);
    close_element(o, "Instrument");
    /* The schema of the first revision requires a calibration date; the
     * creation date stands in for an instrument that was not calibrated. */
    if (d->has_calibration)
    {
        timestamp_leaf(o, "CalibrationDate", &d->calibration);
    }
    else if (!amd1)
    {
        timestamp_leaf(o, "CalibrationDate", &d->date);
    }
    open_element(o, "ProbingSystem");
    leaf_text(o, "Type", probing_text(d->probing));
    token_leaf(o, a, "Identification", d->probing_identification, false);
    close_element(o, "ProbingSystem");
    if (d->comment != NULL && d->comment[0] != '\0')
    {
        leaf_text(o, "Comment", d->comment);
    }
    close_element(o, "Record2");
}

static void write_datums(out *o, const x3pio_document *d)
{
    const size_t absolute = x3pio_document_absolute_count(d);
    const size_t count = x3pio_document_point_count(d);
    open_element(o, "DataList");
    for (size_t i = 0; i < count && o->status == X3PIO_OK; ++i)
    {
        line(o);
        if (d->text_valid != NULL && d->text_valid[i] == 0)
        {
            put_text(o, "<Datum/>");
            continue;
        }
        put_text(o, "<Datum>");
        for (size_t k = 0; k < absolute; ++k)
        {
            if (k != 0)
            {
                put(o, ";", 1);
            }
            char text[X3PIO_NUMBER_CAPACITY];
            const size_t size = x3pio_format_number(
                &o->point, d->text_values[i * absolute + k], text, sizeof text);
            put(o, text, size);
        }
        put_text(o, "</Datum>");
    }
    close_element(o, "DataList");
}

x3pio_status x3pio_xml_build(const x3pio_document *d, x3pio_xml_emit emit,
                             void *user, const char **message)
{
    static const char *const axis_names[3] = {"CX", "CY", "CZ"};
    static const char *const rotation_names[9] = {
        "r11", "r12", "r13", "r21", "r22", "r23", "r31", "r32", "r33"};
    out o = {0};
    o.emit = emit;
    o.user = user;
    o.message = message;
    o.depth = -1;
    o.point = x3pio_decimal_point_now();
    x3pio_buffer_init(&o.buffer, &d->allocator);

    put_text(&o, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
    o.depth = 0;
    line(&o);
    put_text(&o, "<p:ISO5436_2 xmlns:p=\"" TARGET_NAMESPACE "\" "
                 "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
                 "xsi:schemaLocation=\"" TARGET_NAMESPACE " " TARGET_NAMESPACE
                 "/ISO5436_2.xsd\">");
    ++o.depth;
    open_element(&o, "Record1");
    leaf_text(&o, "Revision",
              d->revision == X3PIO_REVISION_AMD1 ? "ISO25178-72:2017/DAM1"
                                                 : "ISO5436 - 2000");
    leaf_text(&o, "FeatureType", feature_text(d->feature));
    open_element(&o, "Axes");
    for (size_t k = 0; k < 3; ++k)
    {
        const char code[2] = {
            d->axes[k].type == X3PIO_AXIS_INCREMENTAL ? 'I' : 'A', '\0'};
        const char data[2] = {type_code(d->axes[k].storage), '\0'};
        open_element(&o, axis_names[k]);
        leaf_text(&o, "AxisType", code);
        leaf_text(&o, "DataType", data);
        leaf_number(&o, "Increment", d->axes[k].increment);
        leaf_number(&o, "Offset", d->transformation.offset[k]);
        close_element(&o, axis_names[k]);
    }
    if (x3pio_transformation_has_rotation(&d->transformation))
    {
        open_element(&o, "Rotation");
        for (size_t i = 0; i < 9; ++i)
        {
            const double v = d->transformation.rotation[i];
            leaf_number(&o, rotation_names[i], x3pio_clamp_unit(v));
        }
        close_element(&o, "Rotation");
    }
    close_element(&o, "Axes");
    close_element(&o, "Record1");

    if (d->has_metadata)
    {
        write_metadata(&o, d);
    }

    open_element(&o, "Record3");
    char number[X3PIO_NUMBER_CAPACITY];
    if (d->dims.list)
    {
        (void)snprintf(number, sizeof number, "%zu", d->dims.size_x);
        leaf_text(&o, "ListDimension", number);
    }
    else
    {
        open_element(&o, "MatrixDimension");
        (void)snprintf(number, sizeof number, "%zu", d->dims.size_x);
        leaf_text(&o, "SizeX", number);
        (void)snprintf(number, sizeof number, "%zu", d->dims.size_y);
        leaf_text(&o, "SizeY", number);
        (void)snprintf(number, sizeof number, "%zu", d->dims.size_z);
        leaf_text(&o, "SizeZ", number);
        close_element(&o, "MatrixDimension");
    }
    if (d->binary)
    {
        open_element(&o, "DataLink");
        leaf_text(&o, "PointDataLink", d->point_path);
        leaf_text(&o, "MD5ChecksumPointData",
                  d->point_md5 != NULL ? d->point_md5 : "");
        if (d->valid_path != NULL && d->valid_path[0] != '\0')
        {
            leaf_text(&o, "ValidPointsLink", d->valid_path);
            leaf_text(&o, "MD5ChecksumValidPoints",
                      d->valid_md5 != NULL ? d->valid_md5 : "");
        }
        close_element(&o, "DataLink");
    }
    else
    {
        write_datums(&o, d);
    }
    close_element(&o, "Record3");

    open_element(&o, "Record4");
    leaf_text(&o, "ChecksumFile",
              d->checksum_file != NULL ? d->checksum_file
                                       : X3PIO_CHECKSUM_FILE);
    close_element(&o, "Record4");
    for (size_t i = 0; i < d->vendor_count; ++i)
    {
        leaf_text(&o, "VendorSpecificID", d->vendor_ids[i]);
    }
    --o.depth;
    put_text(&o, "\n</p:ISO5436_2>\n");
    out_flush(&o);
    x3pio_buffer_free(&o.buffer);
    return o.status;
}
