/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/* The public headers include the headers of the C library and each other, and
 * name no library of the implementation: a program that uses libx3pio needs
 * neither minizip-ng nor Expat. */

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static const char *const allowed[] = {"stdbool.h", "stddef.h", "stdint.h",
                                      "stdio.h",   "time.h",   "float.h",
                                      "limits.h",  "wchar.h"};
static const char *const forbidden[] = {"mz_",     "mz.h", "expat", "XML_",
                                        "minizip", "zlib", "libxml"};
static const char *const headers[] = {"convert.h", "document.h", "file.h",
                                      "layout.h",  "reader.h",   "types.h",
                                      "writer.h",  "x3pio.h"};

static void check_header(const char *name)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/include/x3pio/%s", X3PIO_SOURCE_DIR, name);
    FILE *file = fopen(path, "rb");
    if (file == NULL)
    {
        fprintf(stderr, "%s cannot be read\n", path);
        ++failures;
        return;
    }
    char line[1024];
    while (fgets(line, sizeof line, file) != NULL)
    {
        /* The comments may name the libraries, for example where they say what
         * an allocator does not reach. */
        const char *first = line + strspn(line, " \t");
        const bool comment = first[0] == '*' || first[0] == '/';
        for (size_t i = 0;
             !comment && i < sizeof forbidden / sizeof forbidden[0]; ++i)
        {
            if (strstr(line, forbidden[i]) != NULL)
            {
                fprintf(stderr, "%s mentions %s: %s", name, forbidden[i], line);
                ++failures;
            }
        }
        if (strncmp(line, "#include", 8) != 0)
        {
            continue;
        }
        const char *open = strchr(line, '<');
        const char *close = open != NULL ? strchr(open, '>') : NULL;
        if (open == NULL || close == NULL)
        {
            fprintf(stderr,
                    "%s has an include that is not in angle brackets: %s", name,
                    line);
            ++failures;
            continue;
        }
        char target[200];
        snprintf(target, sizeof target, "%.*s", (int)(close - open - 1),
                 open + 1);
        bool ok = strncmp(target, "x3pio/", 6) == 0;
        for (size_t i = 0; !ok && i < sizeof allowed / sizeof allowed[0]; ++i)
        {
            ok = strcmp(target, allowed[i]) == 0;
        }
        if (!ok)
        {
            fprintf(stderr, "%s includes <%s>\n", name, target);
            ++failures;
        }
    }
    fclose(file);
}

int main(void)
{
    for (size_t i = 0; i < sizeof headers / sizeof headers[0]; ++i)
    {
        check_header(headers[i]);
    }
    if (failures == 0)
    {
        printf("the public headers are clean\n");
    }
    return failures == 0 ? 0 : 1;
}
