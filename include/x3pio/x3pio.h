/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Reading and writing of ISO 25178-72 x3p files.
 *
 * @par Standard
 * The library follows ISO 25178-72:2017 and its Amendment 1:2020. A reader is
 * tolerant of the deviations that occur in files of other software, which are
 * listed in the documentation of the format. A writer is strict: what it writes
 * is valid against the schema of its revision.
 *
 * The public headers include nothing but the headers of the C library. The
 * libraries that the implementation uses, for the zip container and for the XML
 * of main.xml, do not appear in them.
 */

#ifndef X3PIO_X3PIO_H
#define X3PIO_X3PIO_H

#include <x3pio/convert.h>
#include <x3pio/document.h>
#include <x3pio/layout.h>
#include <x3pio/reader.h>
#include <x3pio/types.h>
#include <x3pio/version.h>
#include <x3pio/writer.h>

#endif /* X3PIO_X3PIO_H */
