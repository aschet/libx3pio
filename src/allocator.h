/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Allocation and release of memory through the allocator of the program.
 */

#ifndef X3PIO_SRC_ALLOCATOR_H
#define X3PIO_SRC_ALLOCATOR_H

#include "common.h"

/**
 * Allocates memory through an allocator.
 *
 * @param allocator The allocator, with both callbacks set.
 * @param size The size in bytes. A request of no bytes gives a valid pointer.
 * @return The memory, or null if there is none.
 */
void *x3pio_allocate(const x3pio_allocator *allocator, size_t size);

/**
 * Allocates an array, with the multiplication checked.
 *
 * @param allocator The allocator.
 * @param count The number of elements.
 * @param size The size of an element.
 * @return The memory, or null if there is none or the size overflows.
 */
void *x3pio_allocate_array(const x3pio_allocator *allocator, size_t count,
                           size_t size);

/**
 * Allocates an array of elements that are all zero bytes.
 *
 * @param allocator The allocator.
 * @param count The number of elements.
 * @param size The size of an element.
 * @return The memory, or null if there is none or the size overflows.
 */
void *x3pio_allocate_zeroed(const x3pio_allocator *allocator, size_t count,
                            size_t size);

/**
 * Allocates an array of pointers to strings.
 *
 * @param allocator The allocator.
 * @param count The number of pointers.
 * @return The array, or null if there is none or the size overflows.
 */
char **x3pio_allocate_strings(const x3pio_allocator *allocator, size_t count);

/**
 * Releases an array of pointers to strings, but not the strings.
 *
 * @param allocator The allocator that allocated the array.
 * @param strings The array. May be null.
 */
void x3pio_release_strings(const x3pio_allocator *allocator, char **strings);

/**
 * Copies pointers to strings.
 *
 * @param[out] dst Receives the pointers.
 * @param[in] src The pointers.
 * @param count The number of pointers.
 */
void x3pio_copy_strings(char **dst, char *const *src, size_t count);

/**
 * Releases memory from x3pio_allocate().
 *
 * @param allocator The allocator that allocated the memory.
 * @param memory The memory. May be null.
 */
void x3pio_release(const x3pio_allocator *allocator, void *memory);

/**
 * Takes the allocator argument of a create function.
 *
 * @param given The argument; null selects the default.
 * @param[out] used Receives a copy of the allocator.
 * @return False if the allocator lacks a callback.
 */
bool x3pio_allocator_take(const x3pio_allocator *given, x3pio_allocator *used);

#endif /* X3PIO_SRC_ALLOCATOR_H */
