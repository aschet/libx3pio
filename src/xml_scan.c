/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Scanning main.xml: which element each tag is, and the text of the
 * leaves.
 */

#include "allocator.h"
#include "text.h"
#include "timestamp.h"
#include "xml_state.h"
#include <limits.h>
#include <math.h>

static bool is_name(const char *name, size_t size, const char *expected)
{
    return strlen(expected) == size && memcmp(name, expected, size) == 0;
}

/* Names the element that a tag starts, by its local name and its parent. */
static classified classify(element parent, const char *name, size_t n)
{
    const classified unknown = {E_UNKNOWN, -1};
#define LEAF(text, id)                                                         \
    if (is_name(name, n, text))                                                \
    {                                                                          \
        return (classified){E_LEAF, (id)};                                     \
    }
#define CONTAINER(text, id)                                                    \
    if (is_name(name, n, text))                                                \
    {                                                                          \
        return (classified){(id), -1};                                         \
    }
    switch (parent)
    {
    case E_UNKNOWN:
        break;
    case E_ROOT:
        CONTAINER("Record1", E_RECORD1)
        CONTAINER("Record2", E_RECORD2)
        CONTAINER("Record3", E_RECORD3)
        CONTAINER("Record4", E_RECORD4)
        LEAF("VendorSpecificID", L_VENDOR)
        break;
    case E_RECORD1:
        LEAF("Revision", L_REVISION)
        LEAF("FeatureType", L_FEATURE)
        CONTAINER("Axes", E_AXES)
        CONTAINER("Rotation", E_ROTATION_RECORD)
        break;
    case E_AXES:
        CONTAINER("CX", E_AXIS_X)
        CONTAINER("CY", E_AXIS_Y)
        CONTAINER("CZ", E_AXIS_Z)
        CONTAINER("Rotation", E_ROTATION_AXES)
        break;
    case E_AXIS_X:
    case E_AXIS_Y:
    case E_AXIS_Z:
    {
        const int base = L_AXIS_BASE + 4 * (int)(parent - E_AXIS_X);
        LEAF("AxisType", base + L_AXIS_TYPE)
        LEAF("DataType", base + L_AXIS_DATA)
        LEAF("Increment", base + L_AXIS_INCREMENT)
        LEAF("Offset", base + L_AXIS_OFFSET)
        break;
    }
    case E_ROTATION_AXES:
    case E_ROTATION_RECORD:
    {
        static const char *const names[9] = {"r11", "r12", "r13", "r21", "r22",
                                             "r23", "r31", "r32", "r33"};
        const int base =
            parent == E_ROTATION_AXES ? L_ROTATION_AXES : L_ROTATION_RECORD;
        for (int i = 0; i < 9; ++i)
        {
            LEAF(names[i], base + i)
        }
        break;
    }
    case E_RECORD2:
        LEAF("Date", L_DATE)
        LEAF("Creator", L_CREATOR)
        CONTAINER("Instrument", E_INSTRUMENT)
        LEAF("CalibrationDate", L_CALIBRATION)
        CONTAINER("ProbingSystem", E_PROBING)
        LEAF("Comment", L_COMMENT)
        break;
    case E_INSTRUMENT:
        LEAF("Manufacturer", L_MANUFACTURER)
        LEAF("Model", L_MODEL)
        LEAF("Serial", L_SERIAL)
        LEAF("Version", L_VERSION)
        break;
    case E_PROBING:
        LEAF("Type", L_PROBING_TYPE)
        LEAF("Identification", L_PROBING_ID)
        break;
    case E_RECORD3:
        CONTAINER("MatrixDimension", E_MATRIX)
        LEAF("ListDimension", L_LIST)
        CONTAINER("DataLink", E_DATALINK)
        CONTAINER("DataList", E_DATALIST)
        break;
    case E_MATRIX:
        LEAF("SizeX", L_SIZE_X)
        LEAF("SizeY", L_SIZE_Y)
        LEAF("SizeZ", L_SIZE_Z)
        break;
    case E_DATALINK:
        LEAF("PointDataLink", L_POINT_LINK)
        LEAF("MD5ChecksumPointData", L_POINT_MD5)
        LEAF("ValidPointsLink", L_VALID_LINK)
        LEAF("MD5ChecksumValidPoints", L_VALID_MD5)
        break;
    case E_DATALIST:
        LEAF("Datum", L_DATUM)
        break;
    case E_RECORD4:
        LEAF("ChecksumFile", L_CHECKSUM_FILE)
        break;
    case E_LEAF:
    case E_ELEMENT_COUNT:
        break;
    }
#undef LEAF
#undef CONTAINER
    return unknown;
}

/* Records a failure of the parse and stops Expat. */
void x3pio_xml_fail(state *s, x3pio_status status, const char *text)
{
    if (s->status == X3PIO_OK)
    {
        s->status = x3pio_fail(s->message, status, text);
        XML_StopParser(s->parser, XML_FALSE);
    }
}

void x3pio_xml_fail_memory(state *s)
{
    x3pio_xml_fail(s, X3PIO_ERR_MEMORY, "Out of memory");
}

/* The local name: the name without a namespace prefix. */
static const char *local_name(const char *name, size_t *size)
{
    const char *colon = strrchr(name, ':');
    const char *local = colon != NULL ? colon + 1 : name;
    *size = strlen(local);
    return local;
}

void XMLCALL x3pio_xml_on_entity_declaration(
    void *user, const XML_Char *name, int is_parameter, const XML_Char *value,
    int value_length, const XML_Char *base, const XML_Char *system_id,
    const XML_Char *public_id, const XML_Char *notation)
{
    (void)name;
    (void)is_parameter;
    (void)value;
    (void)value_length;
    (void)base;
    (void)system_id;
    (void)public_id;
    (void)notation;
    x3pio_xml_fail(
        user, X3PIO_ERR_FORMAT,
        "main.xml declares entities, which the library does not expand");
}

void XMLCALL x3pio_xml_on_start(void *user, const XML_Char *name,
                                const XML_Char **attributes)
{
    (void)attributes;
    state *s = user;
    if (s->status != X3PIO_OK)
    {
        return;
    }
    size_t size = 0;
    const char *local = local_name(name, &size);
    classified found = {E_UNKNOWN, -1};
    if (s->depth == 0)
    {
        if (!s->root_seen && is_name(local, size, "ISO5436_2"))
        {
            s->root_seen = true;
            found.element = E_ROOT;
        }
    }
    else if (s->depth <= MAX_DEPTH)
    {
        found = classify(s->stack[s->depth - 1], local, size);
    }
    /* Of an element that appears twice, the first one counts. */
    if (found.element != E_UNKNOWN && found.element != E_LEAF
        && found.element != E_ROOT)
    {
        if (s->container_seen[found.element])
        {
            found = (classified){E_UNKNOWN, -1};
        }
        s->container_seen[found.element] = true;
    }
    if (s->depth < MAX_DEPTH)
    {
        s->stack[s->depth] = found.element;
    }
    ++s->depth;
    if (found.leaf >= 0)
    {
        s->collecting = found.leaf;
        s->collecting_depth = s->depth;
        s->text.size = 0;
    }
}

void XMLCALL x3pio_xml_on_characters(void *user, const XML_Char *data,
                                     int length)
{
    state *s = user;
    if (s->status != X3PIO_OK || s->collecting < 0
        || s->collecting_depth != s->depth)
    {
        return;
    }
    if (s->text_total + (size_t)length > MAX_TEXT_TOTAL)
    {
        x3pio_xml_fail(s, X3PIO_ERR_FORMAT,
                       "main.xml holds too much text in one place");
        return;
    }
    s->text_total += (size_t)length;
    if (!x3pio_buffer_append(&s->text, data, (size_t)length))
    {
        x3pio_xml_fail_memory(s);
    }
}

/* Parses the numbers of a Datum and appends them. */
static void take_datum(state *s)
{
    if (s->datum_count == s->counts_capacity)
    {
        size_t capacity =
            s->counts_capacity != 0 ? s->counts_capacity * 2 : 1024;
        unsigned char *grown = x3pio_allocate(s->allocator, capacity);
        if (grown == NULL)
        {
            x3pio_xml_fail_memory(s);
            return;
        }
        if (s->datum_count > 0)
        {
            memcpy(grown, s->counts, s->datum_count);
        }
        x3pio_release(s->allocator, s->counts);
        s->counts = grown;
        s->counts_capacity = capacity;
    }
    const char *text = (const char *)s->text.data;
    size_t size = s->text.size;
    x3pio_trim(&text, &size);
    size_t fields = 0;
    if (size > 0)
    {
        for (;;)
        {
            const char *semicolon = memchr(text, ';', size);
            const size_t length =
                semicolon != NULL ? (size_t)(semicolon - text) : size;
            double value = 0.0;
            if (fields >= 3
                || !x3pio_parse_number(&s->point, text, length, &value))
            {
                x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                           "A Datum is not a list of at most three numbers");
                s->status = X3PIO_ERR_FORMAT;
                XML_StopParser(s->parser, XML_FALSE);
                return;
            }
            if (s->value_count == s->value_capacity)
            {
                const size_t capacity =
                    s->value_capacity != 0 ? s->value_capacity * 2 : 4096;
                double *grown =
                    x3pio_allocate_array(s->allocator, capacity, sizeof *grown);
                if (grown == NULL)
                {
                    x3pio_xml_fail_memory(s);
                    return;
                }
                if (s->value_count > 0)
                {
                    memcpy(grown, s->values, s->value_count * sizeof *grown);
                }
                x3pio_release(s->allocator, s->values);
                s->values = grown;
                s->value_capacity = capacity;
            }
            s->values[s->value_count++] = value;
            ++fields;
            if (semicolon == NULL)
            {
                break;
            }
            size -= length + 1;
            text += length + 1;
        }
    }
    s->counts[s->datum_count++] = (unsigned char)fields;
}

void XMLCALL x3pio_xml_on_end(void *user, const XML_Char *name)
{
    (void)name;
    state *s = user;
    if (s->status == X3PIO_OK && s->collecting >= 0
        && s->collecting_depth == s->depth)
    {
        const int id = s->collecting;
        if (id == L_DATUM)
        {
            take_datum(s);
        }
        else if (id == L_VENDOR)
        {
            const char *text = (const char *)s->text.data;
            size_t size = s->text.size;
            x3pio_trim(&text, &size);
            if (size > 0 && !x3pio_document_add_vendor(s->document, text, size))
            {
                x3pio_xml_fail_memory(s);
            }
        }
        else if (!s->leaf_seen[id])
        {
            /* The first of an element that appears twice counts. */
            char *copy = x3pio_copy_text(
                s->allocator, (const char *)s->text.data, s->text.size);
            if (copy == NULL)
            {
                x3pio_xml_fail_memory(s);
            }
            else
            {
                s->leaves[id] = copy;
                s->leaf_seen[id] = true;
            }
        }
        s->collecting = -1;
    }
    --s->depth;
}
