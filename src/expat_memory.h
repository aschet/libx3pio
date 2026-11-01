/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief The memory of Expat, taken from the allocator of the parser that runs.
 */

#ifndef X3PIO_SRC_EXPAT_MEMORY_H
#define X3PIO_SRC_EXPAT_MEMORY_H

#include "common.h"
#include <expat.h>

/**
 * The functions for memory that a parser of Expat is created with. Expat's
 * functions take no pointer of the program, so a block remembers the allocator
 * it came from and the allocator that is current in the thread serves new ones.
 */
const XML_Memory_Handling_Suite *x3pio_expat_suite(void);

/**
 * Makes an allocator current in this thread, for the calls into Expat that
 * allocate: creating a parser, parsing and releasing it.
 *
 * @param[in] allocator The allocator.
 * @return The allocator that was current, for x3pio_expat_leave().
 */
const x3pio_allocator *x3pio_expat_enter(const x3pio_allocator *allocator);

/**
 * Makes the allocator that was current before current again.
 *
 * @param[in] saved What x3pio_expat_enter() returned.
 */
void x3pio_expat_leave(const x3pio_allocator *saved);

#endif /* X3PIO_SRC_EXPAT_MEMORY_H */
