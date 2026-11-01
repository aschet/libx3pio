/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The state that the parts of the reader of main.xml share.
 */

#ifndef X3PIO_SRC_XML_STATE_H
#define X3PIO_SRC_XML_STATE_H

#include "buffer.h"
#include "common.h"
#include "document.h"
#include "text.h"
#include <expat.h>

typedef enum element
{
    E_UNKNOWN,
    E_ROOT,
    E_RECORD1,
    E_RECORD2,
    E_RECORD3,
    E_RECORD4,
    E_AXES,
    E_AXIS_X,
    E_AXIS_Y,
    E_AXIS_Z,
    E_ROTATION_AXES,
    E_ROTATION_RECORD,
    E_INSTRUMENT,
    E_PROBING,
    E_MATRIX,
    E_DATALINK,
    E_DATALIST,
    E_LEAF,
    E_ELEMENT_COUNT
} element;

/* The elements that hold only text. */
/* The groups of leaves have a base each, so the enumerators are not all
 * initialized in the same way. */
// NOLINTNEXTLINE(cert-int09-c,readability-enum-initial-value)
typedef enum leaf
{
    L_REVISION,
    L_FEATURE,
    L_AXIS_BASE,                        /* + 4 * axis + L_AXIS_* */
    L_ROTATION_AXES = L_AXIS_BASE + 12, /* + 0..8 */
    L_ROTATION_RECORD = L_ROTATION_AXES + 9,
    L_DATE = L_ROTATION_RECORD + 9,
    L_CREATOR,
    L_MANUFACTURER,
    L_MODEL,
    L_SERIAL,
    L_VERSION,
    L_CALIBRATION,
    L_PROBING_TYPE,
    L_PROBING_ID,
    L_COMMENT,
    L_SIZE_X,
    L_SIZE_Y,
    L_SIZE_Z,
    L_LIST,
    L_POINT_LINK,
    L_POINT_MD5,
    L_VALID_LINK,
    L_VALID_MD5,
    L_CHECKSUM_FILE,
    L_VENDOR,
    L_DATUM,
    L_COUNT
} leaf;

enum
{
    L_AXIS_TYPE = 0,
    L_AXIS_DATA = 1,
    L_AXIS_INCREMENT = 2,
    L_AXIS_OFFSET = 3
};

#define MAX_DEPTH 24
/* The most text of the leaves that one document holds. */
#define MAX_TEXT_TOTAL ((size_t)64 << 20U)
/* The chunk that Expat is fed. */
#define PARSE_CHUNK 65536

typedef struct classified
{
    element element;
    int leaf; /* -1 if the element is not a leaf */
} classified;

typedef struct state
{
    const x3pio_allocator *allocator;
    const char **message;
    x3pio_status status;
    XML_Parser parser;
    x3pio_decimal_point point;

    int depth;
    element stack[MAX_DEPTH];
    int collecting; /* the leaf whose text is collected, or -1 */
    int collecting_depth;
    x3pio_buffer text;
    size_t text_total;

    bool root_seen;
    bool container_seen[E_ELEMENT_COUNT];
    char *leaves[L_COUNT];
    bool leaf_seen[L_COUNT];

    x3pio_document *document;

    double *values;
    size_t value_count;
    size_t value_capacity;
    unsigned char *counts;
    size_t datum_count;
    size_t counts_capacity;
} state;

/** Records a failure of the parse and stops Expat. */
void x3pio_xml_fail(state *s, x3pio_status status, const char *text);

/** Records that an allocation failed. */
void x3pio_xml_fail_memory(state *s);

/** The handler of Expat for the start of an element. */
void XMLCALL x3pio_xml_on_start(void *user, const XML_Char *name,
                                const XML_Char **attributes);

/** The handler of Expat for the end of an element. */
void XMLCALL x3pio_xml_on_end(void *user, const XML_Char *name);

/** The handler of Expat for the text inside an element. */
void XMLCALL x3pio_xml_on_characters(void *user, const XML_Char *data,
                                     int length);

/** The handler of Expat for the declaration of an entity, which is refused. */
void XMLCALL x3pio_xml_on_entity_declaration(
    void *user, const XML_Char *name, int is_parameter, const XML_Char *value,
    int value_length, const XML_Char *base, const XML_Char *system_id,
    const XML_Char *public_id, const XML_Char *notation);

/**
 * Puts the document together from what the parse collected, and checks it.
 *
 * @param[in,out] s The state after the last byte.
 */
x3pio_status x3pio_xml_finish(state *s);

#endif /* X3PIO_SRC_XML_STATE_H */
