/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Reading main.xml with Expat: the parse, fed in chunks.
 */

#include "allocator.h"
#include "expat_memory.h"
#include "text.h"
#include "xml.h"
#include "xml_state.h"

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
                             x3pio_document **document, const char **message)
{
    *document = NULL;
    state s = {0};
    s.allocator = allocator;
    s.message = message;
    s.collecting = -1;
    s.point = x3pio_decimal_point_now();
    x3pio_buffer_init(&s.text, allocator);
    s.document = x3pio_document_allocate(allocator);
    unsigned char *chunk = x3pio_allocate(allocator, PARSE_CHUNK);
    if (s.document == NULL || chunk == NULL)
    {
        x3pio_document_free(s.document);
        x3pio_release(allocator, chunk);
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }

    const x3pio_allocator *saved = x3pio_expat_enter(allocator);
    s.parser = XML_ParserCreate_MM(NULL, x3pio_expat_suite(), NULL);
    if (s.parser == NULL)
    {
        x3pio_expat_leave(saved);
        x3pio_document_free(s.document);
        x3pio_release(allocator, chunk);
        return x3pio_fail(message, X3PIO_ERR_MEMORY, "Out of memory");
    }
    XML_SetUserData(s.parser, &s);
    XML_SetElementHandler(s.parser, x3pio_xml_on_start, x3pio_xml_on_end);
    XML_SetCharacterDataHandler(s.parser, x3pio_xml_on_characters);
    XML_SetEntityDeclHandler(s.parser, x3pio_xml_on_entity_declaration);

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
        x3pio_expat_enter(allocator);
        const enum XML_Status parsed =
            XML_Parse(s.parser, (const char *)chunk, (int)got, last ? 1 : 0);
        x3pio_expat_leave(saved);
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
        status = x3pio_xml_finish(&s);
    }
    x3pio_expat_enter(allocator);
    XML_ParserFree(s.parser);
    x3pio_expat_leave(saved);
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
