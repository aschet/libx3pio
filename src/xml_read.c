/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Reading of main.xml with Expat.
 *
 * The parser is tolerant. It finds elements by their local name, so a root with
 * any namespace prefix or none is accepted, takes them in any order, takes the
 * first of an element that appears twice, and ignores what it does not know.
 * The text of the elements is collected while Expat reads, and the document is
 * put together when the text ends, so main.xml is never held as a whole.
 */

#include "allocator.h"
#include "text.h"
#include "timestamp.h"
#include "xml.h"
#include <expat.h>
#include <limits.h>
#include <math.h>

/* ---- the memory of Expat ------------------------------------------------ */

/* Expat's functions for memory take no pointer of the program, so the allocator
 * of the parser that is running is kept per thread while Expat is called. Each
 * block remembers the allocator it came from, and so can be reallocated and
 * released in any thread. */
static X3PIO_THREAD_LOCAL const x3pio_allocator *current_allocator;

typedef union block_header
{
    struct
    {
        const x3pio_allocator *allocator;
        size_t size;
    } fields;
    max_align_t alignment;
} block_header;

static void *expat_malloc(size_t size)
{
    const x3pio_allocator *allocator = current_allocator;
    if (allocator == NULL || size > SIZE_MAX - sizeof(block_header))
    {
        return NULL;
    }
    block_header *header = x3pio_allocate(allocator, sizeof *header + size);
    if (header == NULL)
    {
        return NULL;
    }
    header->fields.allocator = allocator;
    header->fields.size = size;
    return header + 1;
}

static void expat_free(void *memory)
{
    if (memory != NULL)
    {
        block_header *header = (block_header *)memory - 1;
        x3pio_release(header->fields.allocator, header);
    }
}

static void *expat_realloc(void *memory, size_t size)
{
    if (memory == NULL)
    {
        return expat_malloc(size);
    }
    block_header *old = (block_header *)memory - 1;
    const x3pio_allocator *allocator = old->fields.allocator;
    const x3pio_allocator *saved = current_allocator;
    current_allocator = allocator;
    void *grown = expat_malloc(size);
    current_allocator = saved;
    if (grown == NULL)
    {
        return NULL;
    }
    memcpy(grown, memory, old->fields.size < size ? old->fields.size : size);
    expat_free(memory);
    return grown;
}

/* ---- the elements ------------------------------------------------------- */

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
/* The most text of one Datum: three numbers and their separators. */
#define MAX_DATUM_TEXT ((size_t)1024)
/* The chunk that Expat is fed. */
#define PARSE_CHUNK 65536

typedef struct classified
{
    element element;
    int leaf; /* -1 if the element is not a leaf */
} classified;

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

/* ---- the state of the parse --------------------------------------------- */

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

    /* What the Datum elements show, which are not kept: the numbers are read
     * again when the points are read. */
    size_t datum_count;
    /* The numbers are kept while they fit the budget, so that a file is parsed
     * once. */
    bool keep;
    size_t budget;
    double *values;
    size_t value_count;
    size_t value_capacity;
    unsigned char *counts;
    size_t counts_capacity;
    unsigned field_mask; /* bit n: a Datum has n numbers */
    bool any_empty;
    bool non_integer[3];
    double lowest[3];
    double highest[3];
} state;

/* Records a failure of the parse and stops Expat. */
static void fail(state *s, x3pio_status status, const char *text)
{
    if (s->status == X3PIO_OK)
    {
        s->status = x3pio_fail(s->message, status, text);
        XML_StopParser(s->parser, XML_FALSE);
    }
}

static void fail_memory(state *s)
{
    fail(s, X3PIO_ERR_MEMORY, "Out of memory");
}

/* The local name: the name without a namespace prefix. */
static const char *local_name(const char *name, size_t *size)
{
    const char *colon = strrchr(name, ':');
    const char *local = colon != NULL ? colon + 1 : name;
    *size = strlen(local);
    return local;
}

static void XMLCALL on_entity_declaration(
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
    fail(user, X3PIO_ERR_FORMAT,
         "main.xml declares entities, which the library does not expand");
}

/* Names the element that a tag starts and puts it on the stack. Of an element
 * that appears twice, the first one counts. */
static classified open_element(int *depth, element stack[MAX_DEPTH],
                               bool *root_seen,
                               bool container_seen[E_ELEMENT_COUNT],
                               const XML_Char *name)
{
    size_t size = 0;
    const char *local = local_name(name, &size);
    classified found = {E_UNKNOWN, -1};
    if (*depth == 0)
    {
        if (!*root_seen && is_name(local, size, "ISO5436_2"))
        {
            *root_seen = true;
            found.element = E_ROOT;
        }
    }
    else if (*depth <= MAX_DEPTH)
    {
        found = classify(stack[*depth - 1], local, size);
    }
    if (found.element != E_UNKNOWN && found.element != E_LEAF
        && found.element != E_ROOT)
    {
        if (container_seen[found.element])
        {
            found = (classified){E_UNKNOWN, -1};
        }
        container_seen[found.element] = true;
    }
    if (*depth < MAX_DEPTH)
    {
        stack[*depth] = found.element;
    }
    ++*depth;
    return found;
}

static void XMLCALL on_start(void *user, const XML_Char *name,
                             const XML_Char **attributes)
{
    (void)attributes;
    state *s = user;
    if (s->status != X3PIO_OK)
    {
        return;
    }
    const classified found = open_element(&s->depth, s->stack, &s->root_seen,
                                          s->container_seen, name);
    if (found.leaf >= 0)
    {
        s->collecting = found.leaf;
        s->collecting_depth = s->depth;
        s->text.size = 0;
    }
}

static void XMLCALL on_characters(void *user, const XML_Char *data, int length)
{
    state *s = user;
    if (s->status != X3PIO_OK || s->collecting < 0
        || s->collecting_depth != s->depth)
    {
        return;
    }
    if (s->collecting == L_DATUM)
    {
        /* The numbers of a Datum are not kept, so their text does not count. */
        if (s->text.size + (size_t)length > MAX_DATUM_TEXT)
        {
            fail(s, X3PIO_ERR_FORMAT,
                 "A Datum is not a list of at most three numbers");
            return;
        }
    }
    else
    {
        if (s->text_total + (size_t)length > MAX_TEXT_TOTAL)
        {
            fail(s, X3PIO_ERR_FORMAT,
                 "main.xml holds too much text in one place");
            return;
        }
        s->text_total += (size_t)length;
    }
    if (!x3pio_buffer_append(&s->text, data, (size_t)length))
    {
        fail_memory(s);
    }
}

/* Splits the text of a Datum into at most three numbers. */
static bool parse_datum(const x3pio_decimal_point *point, const char *text,
                        size_t size, double values[3], size_t *fields)
{
    x3pio_trim(&text, &size);
    *fields = 0;
    if (size == 0)
    {
        return true;
    }
    for (;;)
    {
        const char *semicolon = memchr(text, ';', size);
        const size_t length =
            semicolon != NULL ? (size_t)(semicolon - text) : size;
        double value = 0.0;
        if (*fields >= 3 || !x3pio_parse_number(point, text, length, &value))
        {
            return false;
        }
        values[(*fields)++] = value;
        if (semicolon == NULL)
        {
            return true;
        }
        size -= length + 1;
        text += length + 1;
    }
}

/* Keeps the numbers of a Datum, or gives up all that were kept if they would
 * take more than the budget. They are held twice while the document is put
 * together, which the budget counts. */
static void keep_datum(state *s, const double numbers[3], size_t fields)
{
    const size_t held =
        (s->value_count + fields) * sizeof(double) + s->datum_count;
    if (held > s->budget / 2)
    {
        x3pio_release(s->allocator, s->values);
        x3pio_release(s->allocator, s->counts);
        s->values = NULL;
        s->counts = NULL;
        s->value_count = s->value_capacity = s->counts_capacity = 0;
        s->keep = false;
        return;
    }
    if (s->datum_count > s->counts_capacity)
    {
        const size_t capacity =
            s->counts_capacity != 0 ? s->counts_capacity * 2 : 1024;
        unsigned char *grown = x3pio_allocate(s->allocator, capacity);
        if (grown == NULL)
        {
            fail_memory(s);
            return;
        }
        if (s->datum_count > 1)
        {
            memcpy(grown, s->counts, s->datum_count - 1);
        }
        x3pio_release(s->allocator, s->counts);
        s->counts = grown;
        s->counts_capacity = capacity;
    }
    if (s->value_count + fields > s->value_capacity)
    {
        const size_t capacity =
            s->value_capacity != 0 ? s->value_capacity * 2 : 4096;
        double *grown =
            x3pio_allocate_array(s->allocator, capacity, sizeof *grown);
        if (grown == NULL)
        {
            fail_memory(s);
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
    for (size_t k = 0; k < fields; ++k)
    {
        s->values[s->value_count++] = numbers[k];
    }
    s->counts[s->datum_count - 1] = (unsigned char)fields;
}

/* Checks the numbers of a Datum and notes what the others have to agree on:
 * the axes are known only at the end of the document. */
static void take_datum(state *s)
{
    double values[3] = {0.0, 0.0, 0.0};
    size_t fields = 0;
    if (!parse_datum(&s->point, (const char *)s->text.data, s->text.size,
                     values, &fields))
    {
        fail(s, X3PIO_ERR_FORMAT,
             "A Datum is not a list of at most three numbers");
        return;
    }
    ++s->datum_count;
    s->field_mask |= 1U << fields;
    if (s->keep)
    {
        keep_datum(s, values, fields);
    }
    if (fields == 0)
    {
        s->any_empty = true;
        return;
    }
    for (size_t k = 0; k < fields; ++k)
    {
        if (values[k] != floor(values[k]))
        {
            s->non_integer[k] = true;
        }
        if (values[k] < s->lowest[k])
        {
            s->lowest[k] = values[k];
        }
        if (values[k] > s->highest[k])
        {
            s->highest[k] = values[k];
        }
    }
}

static void XMLCALL on_end(void *user, const XML_Char *name)
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
                fail_memory(s);
            }
        }
        else if (!s->leaf_seen[id])
        {
            /* The first of an element that appears twice counts. */
            char *copy = x3pio_copy_text(
                s->allocator, (const char *)s->text.data, s->text.size);
            if (copy == NULL)
            {
                fail_memory(s);
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

/* ---- putting the document together -------------------------------------- */

/* The text of a leaf, without white space around it; empty if it is missing. */
static const char *leaf_text(const state *s, int id, size_t *size)
{
    const char *text = s->leaves[id] != NULL ? s->leaves[id] : "";
    *size = strlen(text);
    x3pio_trim(&text, size);
    return text;
}

static bool have(const state *s, int id)
{
    return s->leaf_seen[id];
}

/* A number from a leaf; the fallback if the leaf is missing or empty. */
static x3pio_status number_of(state *s, int id, double fallback, double *out)
{
    size_t size = 0;
    const char *text = leaf_text(s, id, &size);
    if (size == 0)
    {
        *out = fallback;
        return X3PIO_OK;
    }
    double value = 0.0;
    if (!x3pio_parse_number(&s->point, text, size, &value) || !isfinite(value))
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "A number of main.xml is not valid");
    }
    *out = value;
    return X3PIO_OK;
}

static x3pio_status size_of_leaf(state *s, int id, size_t *out)
{
    size_t size = 0;
    const char *text = leaf_text(s, id, &size);
    double value = 0.0;
    if (!x3pio_parse_number(&s->point, text, size, &value) || !(value >= 0.0)
        || value != floor(value) || value > 9007199254740992.0
        || value > (double)SIZE_MAX)
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "A size of main.xml is not valid");
    }
    *out = (size_t)value;
    return X3PIO_OK;
}

/* The dashes that appear in real files instead of '-': U+2010 to U+2015 and the
 * minus sign U+2212, in UTF-8. */
static bool revision_is_amd1(const char *text, size_t size)
{
    static const char expected[] = "ISO25178-72:2017/DAM1";
    char normalized[sizeof expected + 8];
    size_t used = 0;
    for (size_t i = 0; i < size; ++i)
    {
        const unsigned char c0 = (unsigned char)text[i];
        char c = (char)c0;
        if (c0 == 0xE2 && i + 2 < size)
        {
            const unsigned char c1 = (unsigned char)text[i + 1];
            const unsigned char c2 = (unsigned char)text[i + 2];
            if ((c1 == 0x80 && c2 >= 0x90 && c2 <= 0x95)
                || (c1 == 0x88 && c2 == 0x92))
            {
                c = '-';
                i += 2;
            }
        }
        if (used >= sizeof normalized)
        {
            return false;
        }
        normalized[used++] = c;
    }
    return used == sizeof expected - 1
           && memcmp(normalized, expected, used) == 0;
}

static bool parse_code(const char *text, size_t size, const char *codes,
                       size_t *index)
{
    if (size != 1)
    {
        return false;
    }
    const char c =
        (char)((text[0] >= 'a' && text[0] <= 'z') ? text[0] - 'a' + 'A'
                                                  : text[0]);
    const char *found = strchr(codes, c);
    if (found == NULL || c == '\0')
    {
        return false;
    }
    *index = (size_t)(found - codes);
    return true;
}

static x3pio_status build_axis(state *s, size_t k, double *offset)
{
    x3pio_axis *axis = &s->document->axes[k];
    const int base = L_AXIS_BASE + 4 * (int)k;
    size_t size = 0;
    if (!have(s, base + L_AXIS_TYPE))
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "An axis has no AxisType");
    }
    const char *text = leaf_text(s, base + L_AXIS_TYPE, &size);
    size_t index = 0;
    if (!parse_code(text, size, "IA", &index))
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "An axis has an unknown AxisType");
    }
    axis->type = index == 0 ? X3PIO_AXIS_INCREMENTAL : X3PIO_AXIS_ABSOLUTE;
    text = leaf_text(s, base + L_AXIS_DATA, &size);
    axis->storage = X3PIO_TYPE_DOUBLE;
    if (size == 0)
    {
        if (axis->type == X3PIO_AXIS_ABSOLUTE)
        {
            return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                              "An absolute axis has no DataType");
        }
    }
    else if (parse_code(text, size, "ILFD", &index))
    {
        static const x3pio_type types[4] = {X3PIO_TYPE_INT16, X3PIO_TYPE_INT32,
                                            X3PIO_TYPE_FLOAT,
                                            X3PIO_TYPE_DOUBLE};
        axis->storage = types[index];
    }
    else
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "An axis has an unknown DataType");
    }
    x3pio_status status =
        number_of(s, base + L_AXIS_INCREMENT, 1.0, &axis->increment);
    if (status != X3PIO_OK)
    {
        return status;
    }
    if (axis->increment == 0.0)
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "An axis has an Increment of zero");
    }
    return number_of(s, base + L_AXIS_OFFSET, 0.0, offset);
}

static x3pio_status build_rotation(state *s, int base, x3pio_placement *place)
{
    for (int i = 0; i < 9; ++i)
    {
        if (!have(s, base + i))
        {
            return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                              "The rotation lacks an element");
        }
        double v = 0.0;
        const x3pio_status status = number_of(s, base + i, 0.0, &v);
        if (status != X3PIO_OK)
        {
            return status;
        }
        /* Elements a hair beyond +-1 come from rounding when writing. */
        if (fabs(v) > 1.0 + 1e-6)
        {
            return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                              "A rotation element lies outside [-1, 1]");
        }
        place->rotation[i] = x3pio_clamp_unit(v);
    }
    return X3PIO_OK;
}

static bool any_of(const state *s, int base, int count)
{
    for (int i = 0; i < count; ++i)
    {
        if (have(s, base + i))
        {
            return true;
        }
    }
    return false;
}

static bool assign(state *s, char **field, int id, bool raw)
{
    size_t size = 0;
    const char *text;
    if (raw)
    {
        text = s->leaves[id] != NULL ? s->leaves[id] : "";
        size = strlen(text);
    }
    else
    {
        text = leaf_text(s, id, &size);
    }
    if (!x3pio_document_assign(s->document, field, text, size))
    {
        fail_memory(s);
        return false;
    }
    return true;
}

static x3pio_status build_metadata(state *s)
{
    x3pio_document *d = s->document;
    d->has_metadata = true;
    size_t size = 0;
    const char *text = leaf_text(s, L_DATE, &size);
    char buffer[64];
    /* A date that is no date, such as "N/A", is no date. */
    if (size < sizeof buffer)
    {
        memcpy(buffer, text, size);
        buffer[size] = '\0';
        d->has_date = x3pio_timestamp_parse(buffer, &d->date);
    }
    text = leaf_text(s, L_CALIBRATION, &size);
    if (size < sizeof buffer)
    {
        memcpy(buffer, text, size);
        buffer[size] = '\0';
        d->has_calibration = x3pio_timestamp_parse(buffer, &d->calibration);
    }
    text = leaf_text(s, L_PROBING_TYPE, &size);
    d->probing = X3PIO_PROBING_UNKNOWN;
    if (x3pio_equals_nocase(text, size, "contacting"))
    {
        d->probing = X3PIO_PROBING_CONTACTING;
    }
    else if (x3pio_equals_nocase(text, size, "noncontacting"))
    {
        d->probing = X3PIO_PROBING_NON_CONTACTING;
    }
    else if (x3pio_equals_nocase(text, size, "software"))
    {
        d->probing = X3PIO_PROBING_SOFTWARE;
    }
    if (!assign(s, &d->creator, L_CREATOR, false)
        || !assign(s, &d->manufacturer, L_MANUFACTURER, false)
        || !assign(s, &d->model, L_MODEL, false)
        || !assign(s, &d->serial, L_SERIAL, false)
        || !assign(s, &d->version, L_VERSION, false)
        || !assign(s, &d->probing_identification, L_PROBING_ID, false)
        || !assign(s, &d->comment, L_COMMENT, true))
    {
        return X3PIO_ERR_MEMORY;
    }
    return X3PIO_OK;
}

/* Moves the numbers that were kept into the document: a row of the stored axes
 * for each point, NaN for an empty Datum, and a byte that tells the points that
 * were measured. */
static x3pio_status hold_datums(state *s, size_t absolute)
{
    x3pio_document *d = s->document;
    const size_t count = s->datum_count;
    size_t total = 0;
    if (!x3pio_multiply(count, absolute, &total))
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "The dimensions of the points are too large");
    }
    d->text_values = x3pio_allocate_array(&d->allocator, total != 0 ? total : 1,
                                          sizeof(double));
    d->text_valid = x3pio_allocate(&d->allocator, count != 0 ? count : 1);
    if (d->text_values == NULL || d->text_valid == NULL)
    {
        return x3pio_fail(s->message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    d->text_value_count = total;
    size_t from = 0;
    bool all_valid = true;
    for (size_t i = 0; i < count; ++i)
    {
        if (s->counts[i] == 0)
        {
            for (size_t k = 0; k < absolute; ++k)
            {
                d->text_values[i * absolute + k] = NAN;
            }
            d->text_valid[i] = 0;
            all_valid = false;
            continue;
        }
        for (size_t k = 0; k < absolute; ++k)
        {
            d->text_values[i * absolute + k] = s->values[from + k];
        }
        from += absolute;
        d->text_valid[i] = 1;
    }
    if (all_valid)
    {
        x3pio_release(&d->allocator, d->text_valid);
        d->text_valid = NULL;
    }
    return X3PIO_OK;
}

/* Checks what the Datum elements showed against the axes. The numbers are kept
 * if they fit the budget, and otherwise read again when the points are read. */
static x3pio_status check_datums(state *s)
{
    x3pio_document *d = s->document;
    const size_t absolute = x3pio_document_absolute_count(d);
    if (s->datum_count != x3pio_document_point_count(d))
    {
        return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                          "DataList does not hold a Datum for every point");
    }
    /* A Datum is empty, for a point that was not measured, or holds a number
     * for every axis that is stored. */
    if ((s->field_mask & ~(1U | (1U << absolute))) != 0)
    {
        return x3pio_fail(
            s->message, X3PIO_ERR_FORMAT,
            "A Datum does not hold a number for every axis that is stored");
    }
    x3pio_coordinate axes[3];
    size_t axis_count = 0;
    for (size_t k = 0; k < 3; ++k)
    {
        if (d->axes[k].type == X3PIO_AXIS_ABSOLUTE)
        {
            axes[axis_count++] = (x3pio_coordinate)k;
        }
    }
    for (size_t k = 0; k < axis_count; ++k)
    {
        const x3pio_type type = d->axes[axes[k]].storage;
        if (type == X3PIO_TYPE_INT16 || type == X3PIO_TYPE_INT32)
        {
            const double limit =
                type == X3PIO_TYPE_INT16 ? 32768.0 : 2147483648.0;
            if (s->non_integer[k] || s->lowest[k] < -limit
                || s->highest[k] >= limit)
            {
                return x3pio_fail(s->message, X3PIO_ERR_FORMAT,
                                  "A Datum holds a number that is no value "
                                  "of an integer axis");
            }
        }
    }
    d->text_has_missing = s->any_empty;
    return s->keep ? hold_datums(s, absolute) : X3PIO_OK;
}

static x3pio_status finish(state *s)
{
    x3pio_document *d = s->document;
    const char **m = s->message;
    if (!s->root_seen)
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT,
                          "main.xml has no ISO5436_2 element");
    }
    if (!s->container_seen[E_RECORD1])
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT, "main.xml has no Record1");
    }
    if (!s->container_seen[E_AXES])
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT, "Record1 has no Axes");
    }
    double offsets[3] = {0, 0, 0};
    for (size_t k = 0; k < 3; ++k)
    {
        if (!s->container_seen[E_AXIS_X + k])
        {
            return x3pio_fail(m, X3PIO_ERR_FORMAT,
                              "Axes lacks one of CX, CY and CZ");
        }
        const x3pio_status status = build_axis(s, k, &offsets[k]);
        if (status != X3PIO_OK)
        {
            return status;
        }
    }
    memcpy(d->placement.offset, offsets, sizeof offsets);
    if (d->axes[2].type != X3PIO_AXIS_ABSOLUTE)
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT, "The z axis must be absolute");
    }
    x3pio_status status = X3PIO_OK;
    if (any_of(s, L_ROTATION_AXES, 9) || s->container_seen[E_ROTATION_AXES])
    {
        status = build_rotation(s, L_ROTATION_AXES, &d->placement);
    }
    else if (any_of(s, L_ROTATION_RECORD, 9)
             || s->container_seen[E_ROTATION_RECORD])
    {
        status = build_rotation(s, L_ROTATION_RECORD, &d->placement);
    }
    if (status != X3PIO_OK)
    {
        return status;
    }
    if (!have(s, L_FEATURE))
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT, "Record1 has no FeatureType");
    }
    size_t size = 0;
    const char *text = leaf_text(s, L_FEATURE, &size);
    x3pio_feature feature = X3PIO_SURFACE;
    if (x3pio_equals_nocase(text, size, "prf"))
    {
        feature = X3PIO_PROFILE;
    }
    else if (x3pio_equals_nocase(text, size, "sur"))
    {
        feature = X3PIO_SURFACE;
    }
    else if (x3pio_equals_nocase(text, size, "pcl"))
    {
        feature = X3PIO_POINT_CLOUD;
    }
    else
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT,
                          "Record1 has an unknown FeatureType");
    }
    if (s->container_seen[E_RECORD2])
    {
        status = build_metadata(s);
        if (status != X3PIO_OK)
        {
            return status;
        }
    }
    if (!s->container_seen[E_RECORD3])
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT, "main.xml has no Record3");
    }
    const bool matrix = s->container_seen[E_MATRIX];
    const bool list = have(s, L_LIST);
    if (matrix == list)
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT,
                          "Record3 needs exactly one of MatrixDimension and "
                          "ListDimension");
    }
    if (matrix)
    {
        size_t u = 0;
        size_t v = 0;
        size_t w = 0;
        if (!have(s, L_SIZE_X) || !have(s, L_SIZE_Y) || !have(s, L_SIZE_Z))
        {
            return x3pio_fail(m, X3PIO_ERR_FORMAT,
                              "MatrixDimension needs SizeX, SizeY and SizeZ");
        }
        status = size_of_leaf(s, L_SIZE_X, &u);
        if (status == X3PIO_OK)
        {
            status = size_of_leaf(s, L_SIZE_Y, &v);
        }
        if (status == X3PIO_OK)
        {
            status = size_of_leaf(s, L_SIZE_Z, &w);
        }
        if (status != X3PIO_OK)
        {
            return status;
        }
        d->dims = (x3pio_dimensions){u, v, w, false};
    }
    else
    {
        size_t count = 0;
        status = size_of_leaf(s, L_LIST, &count);
        if (status != X3PIO_OK)
        {
            return status;
        }
        d->dims = (x3pio_dimensions){count, 1, 1, true};
    }
    /* The data is what counts if the feature type disagrees with it. */
    d->feature = feature;
    if (d->dims.list)
    {
        d->feature = X3PIO_POINT_CLOUD;
    }
    else if (d->feature == X3PIO_POINT_CLOUD
             || (d->feature == X3PIO_PROFILE && d->dims.size_y != 1))
    {
        d->feature = X3PIO_SURFACE;
    }
    text = leaf_text(s, L_REVISION, &size);
    d->revision = d->vendor_count > 1 || revision_is_amd1(text, size)
                      ? X3PIO_REVISION_AMD1
                      : X3PIO_REVISION_2017;

    const bool link = s->container_seen[E_DATALINK];
    if (link == s->container_seen[E_DATALIST])
    {
        return x3pio_fail(m, X3PIO_ERR_FORMAT,
                          "Record3 needs exactly one of DataLink and DataList");
    }
    if (link)
    {
        if (!have(s, L_POINT_LINK) || leaf_text(s, L_POINT_LINK, &size) == NULL
            || size == 0)
        {
            return x3pio_fail(m, X3PIO_ERR_FORMAT,
                              have(s, L_POINT_LINK) ? "PointDataLink is empty"
                                                    : "DataLink has no "
                                                      "PointDataLink");
        }
        d->binary = true;
        if (!assign(s, &d->point_path, L_POINT_LINK, false)
            || !assign(s, &d->point_md5, L_POINT_MD5, false)
            || !assign(s, &d->valid_path, L_VALID_LINK, false)
            || !assign(s, &d->valid_md5, L_VALID_MD5, false))
        {
            return X3PIO_ERR_MEMORY;
        }
    }
    else
    {
        status = check_datums(s);
        if (status != X3PIO_OK)
        {
            return status;
        }
    }
    text = leaf_text(s, L_CHECKSUM_FILE, &size);
    if (!x3pio_document_assign(d, &d->checksum_file,
                               size > 0 ? text : "md5checksum.hex",
                               size > 0 ? size : strlen("md5checksum.hex")))
    {
        return X3PIO_ERR_MEMORY;
    }
    return X3PIO_OK;
}

/* ---- the parse ----------------------------------------------------------- */

static void release_state(state *s)
{
    for (int i = 0; i < L_COUNT; ++i)
    {
        x3pio_release(s->allocator, s->leaves[i]);
    }
    x3pio_release(s->allocator, s->values);
    x3pio_release(s->allocator, s->counts);
    x3pio_buffer_free(&s->text);
}

x3pio_status x3pio_xml_parse(const x3pio_allocator *allocator,
                             x3pio_xml_pull pull, void *user,
                             size_t text_budget, x3pio_document **document,
                             const char **message)
{
    *document = NULL;
    state s = {0};
    s.allocator = allocator;
    s.message = message;
    s.collecting = -1;
    s.budget = text_budget;
    s.keep = text_budget != 0;
    s.point = x3pio_decimal_point_now();
    for (size_t k = 0; k < 3; ++k)
    {
        s.lowest[k] = INFINITY;
        s.highest[k] = -INFINITY;
    }
    x3pio_buffer_init(&s.text, allocator);
    s.document = x3pio_document_make(allocator);
    unsigned char *chunk = x3pio_allocate(allocator, PARSE_CHUNK);
    if (s.document == NULL || chunk == NULL)
    {
        x3pio_document_free(s.document);
        x3pio_release(allocator, chunk);
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }

    static const XML_Memory_Handling_Suite suite = {expat_malloc, expat_realloc,
                                                    expat_free};
    const x3pio_allocator *saved = current_allocator;
    current_allocator = allocator;
    s.parser = XML_ParserCreate_MM(NULL, &suite, NULL);
    if (s.parser == NULL)
    {
        current_allocator = saved;
        x3pio_document_free(s.document);
        x3pio_release(allocator, chunk);
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    XML_SetUserData(s.parser, &s);
    XML_SetElementHandler(s.parser, on_start, on_end);
    XML_SetCharacterDataHandler(s.parser, on_characters);
    XML_SetEntityDeclHandler(s.parser, on_entity_declaration);

    x3pio_status status = X3PIO_OK;
    for (;;)
    {
        size_t got = 0;
        status = pull(user, chunk, PARSE_CHUNK, &got, message);
        if (status != X3PIO_OK)
        {
            break;
        }
        const bool last = got == 0;
        current_allocator = allocator;
        const enum XML_Status parsed =
            XML_Parse(s.parser, (const char *)chunk, (int)got, last ? 1 : 0);
        current_allocator = saved;
        if (s.status != X3PIO_OK)
        {
            status = s.status;
            break;
        }
        if (parsed == XML_STATUS_ERROR)
        {
            status =
                XML_GetErrorCode(s.parser) == XML_ERROR_NO_MEMORY
                    ? x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory")
                    : x3pio_fail(message, X3PIO_ERR_FORMAT,
                                 XML_ErrorString(XML_GetErrorCode(s.parser)));
            break;
        }
        if (last)
        {
            break;
        }
    }
    if (status == X3PIO_OK)
    {
        status = finish(&s);
    }
    current_allocator = allocator;
    XML_ParserFree(s.parser);
    current_allocator = saved;
    x3pio_release(allocator, chunk);
    release_state(&s);
    if (status != X3PIO_OK)
    {
        x3pio_document_free(s.document);
        return status;
    }
    *document = s.document;
    return X3PIO_OK;
}

/* ---- the Datum elements, a part at a time ------------------------------- */

/* The parse of the points of a text file. x3pio_xml_parse() has checked the
 * whole file and counted the Datum elements; this parse goes through main.xml
 * again and stops whenever it has delivered the points that were asked for, so
 * the points of a file in text are never held as a whole. */
struct x3pio_xml_datums
{
    const x3pio_allocator *allocator;
    const char **message;
    x3pio_status status;
    XML_Parser parser;
    x3pio_decimal_point point;
    x3pio_xml_pull pull;
    void *user;
    unsigned char *chunk;
    bool suspended;
    bool finished;
    /* The parse stops at the first Datum, so that the buffers of Expat and the
     * text exist before the points are read. */
    bool priming;

    int depth;
    element stack[MAX_DEPTH];
    bool root_seen;
    bool container_seen[E_ELEMENT_COUNT];
    bool collecting;
    int collecting_depth;
    x3pio_buffer text;

    size_t absolute;
    double *values;
    unsigned char *valid;
    size_t want;
    size_t got;
};

static void datums_fail(x3pio_xml_datums *d, x3pio_status status,
                        const char *text)
{
    if (d->status == X3PIO_OK)
    {
        d->status = x3pio_fail(d->message, status, text);
        XML_StopParser(d->parser, XML_FALSE);
    }
}

static void XMLCALL datums_on_start(void *user, const XML_Char *name,
                                    const XML_Char **attributes)
{
    (void)attributes;
    x3pio_xml_datums *d = user;
    if (d->status != X3PIO_OK)
    {
        return;
    }
    const classified found = open_element(&d->depth, d->stack, &d->root_seen,
                                          d->container_seen, name);
    if (found.leaf == L_DATUM)
    {
        d->collecting = true;
        d->collecting_depth = d->depth;
        d->text.size = 0;
        if (d->priming)
        {
            d->priming = false;
            XML_StopParser(d->parser, XML_TRUE);
        }
    }
}

static void XMLCALL datums_on_characters(void *user, const XML_Char *data,
                                         int length)
{
    x3pio_xml_datums *d = user;
    if (d->status != X3PIO_OK || !d->collecting
        || d->collecting_depth != d->depth)
    {
        return;
    }
    if (d->text.size + (size_t)length > MAX_DATUM_TEXT)
    {
        datums_fail(d, X3PIO_ERR_FORMAT,
                    "A Datum is not a list of at most three numbers");
    }
    else if (!x3pio_buffer_append(&d->text, data, (size_t)length))
    {
        datums_fail(d, X3PIO_ERR_MEMORY, "Out of memory");
    }
}

static void XMLCALL datums_on_end(void *user, const XML_Char *name)
{
    (void)name;
    x3pio_xml_datums *d = user;
    if (d->status == X3PIO_OK && d->collecting
        && d->collecting_depth == d->depth)
    {
        d->collecting = false;
        double numbers[3] = {0.0, 0.0, 0.0};
        size_t fields = 0;
        if (d->got >= d->want)
        {
            datums_fail(d, X3PIO_ERR_FORMAT,
                        "DataList holds more Datum elements than points");
        }
        else if (!parse_datum(&d->point, (const char *)d->text.data,
                              d->text.size, numbers, &fields))
        {
            datums_fail(d, X3PIO_ERR_FORMAT,
                        "A Datum is not a list of at most three numbers");
        }
        else if (fields != 0 && fields != d->absolute)
        {
            datums_fail(d, X3PIO_ERR_FORMAT,
                        "A Datum does not hold a number for every axis that "
                        "is stored");
        }
        else
        {
            double *at = d->values + d->got * d->absolute;
            for (size_t k = 0; k < d->absolute; ++k)
            {
                at[k] = fields == 0 ? NAN : numbers[k];
            }
            d->valid[d->got] = fields != 0 ? 1U : 0U;
            ++d->got;
            if (d->got == d->want)
            {
                XML_StopParser(d->parser, XML_TRUE);
            }
        }
    }
    --d->depth;
}

static x3pio_status datums_run(x3pio_xml_datums *d, bool priming,
                               const char **message);

x3pio_status x3pio_xml_datums_open(const x3pio_allocator *allocator,
                                   x3pio_xml_pull pull, void *user,
                                   size_t absolute, x3pio_xml_datums **datums,
                                   const char **message)
{
    *datums = NULL;
    x3pio_xml_datums *d = x3pio_allocate(allocator, sizeof *d);
    if (d == NULL)
    {
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    *d = (x3pio_xml_datums){0};
    d->allocator = allocator;
    d->message = message;
    d->pull = pull;
    d->user = user;
    d->absolute = absolute;
    d->point = x3pio_decimal_point_now();
    x3pio_buffer_init(&d->text, allocator);
    d->chunk = x3pio_allocate(allocator, PARSE_CHUNK);
    static const XML_Memory_Handling_Suite suite = {expat_malloc, expat_realloc,
                                                    expat_free};
    const x3pio_allocator *saved = current_allocator;
    current_allocator = allocator;
    d->parser =
        d->chunk != NULL ? XML_ParserCreate_MM(NULL, &suite, NULL) : NULL;
    current_allocator = saved;
    if (d->parser == NULL)
    {
        x3pio_xml_datums_free(d);
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    XML_SetUserData(d->parser, d);
    XML_SetElementHandler(d->parser, datums_on_start, datums_on_end);
    XML_SetCharacterDataHandler(d->parser, datums_on_characters);
    /* The buffers that Expat and the text need exist now, and the parse goes
     * up to the first Datum, so that reading the points allocates nothing: a
     * chunk and the start of the next one, cut in the middle of a Datum, fit
     * the buffer of Expat. */
    current_allocator = allocator;
    const bool room = XML_GetBuffer(d->parser, 2 * PARSE_CHUNK) != NULL;
    current_allocator = saved;
    d->priming = true;
    x3pio_status status =
        room && x3pio_buffer_reserve(&d->text, MAX_DATUM_TEXT)
            ? datums_run(d, true, message)
            : x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    if (status != X3PIO_OK)
    {
        x3pio_xml_datums_free(d);
        return status;
    }
    *datums = d;
    return X3PIO_OK;
}

/* Feeds the parser until it stops or `done` says that enough was delivered. */
static x3pio_status datums_run(x3pio_xml_datums *d, bool priming,
                               const char **message)
{
    const x3pio_allocator *saved = current_allocator;
    while (priming ? d->priming && !d->finished : d->got < d->want)
    {
        enum XML_Status parsed = XML_STATUS_OK;
        if (d->suspended)
        {
            current_allocator = d->allocator;
            parsed = XML_ResumeParser(d->parser);
            current_allocator = saved;
        }
        else if (d->finished)
        {
            return x3pio_fail(message, X3PIO_ERR_FORMAT,
                              "DataList holds fewer Datum elements than "
                              "points");
        }
        else
        {
            size_t got = 0;
            const x3pio_status status =
                d->pull(d->user, d->chunk, PARSE_CHUNK, &got, message);
            if (status != X3PIO_OK)
            {
                return status;
            }
            d->finished = got == 0;
            current_allocator = d->allocator;
            parsed = XML_Parse(d->parser, (const char *)d->chunk, (int)got,
                               d->finished ? 1 : 0);
            current_allocator = saved;
        }
        if (d->status != X3PIO_OK)
        {
            return d->status;
        }
        if (parsed == XML_STATUS_ERROR)
        {
            return XML_GetErrorCode(d->parser) == XML_ERROR_NO_MEMORY
                       ? x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory")
                       : x3pio_fail(
                             message, X3PIO_ERR_FORMAT,
                             XML_ErrorString(XML_GetErrorCode(d->parser)));
        }
        XML_ParsingStatus status;
        XML_GetParsingStatus(d->parser, &status);
        d->suspended = status.parsing == XML_SUSPENDED;
    }
    return X3PIO_OK;
}

x3pio_status x3pio_xml_datums_read(x3pio_xml_datums *d, size_t n,
                                   double *values, unsigned char *valid,
                                   const char **message)
{
    d->message = message;
    d->values = values;
    d->valid = valid;
    d->want = n;
    d->got = 0;
    return datums_run(d, false, message);
}

void x3pio_xml_datums_free(x3pio_xml_datums *datums)
{
    if (datums == NULL)
    {
        return;
    }
    const x3pio_allocator allocator = *datums->allocator;
    if (datums->parser != NULL)
    {
        const x3pio_allocator *saved = current_allocator;
        current_allocator = &allocator;
        XML_ParserFree(datums->parser);
        current_allocator = saved;
    }
    x3pio_buffer_free(&datums->text);
    x3pio_release(&allocator, datums->chunk);
    x3pio_release(&allocator, datums);
}
