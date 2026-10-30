/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * @brief Checks of timestamps.
 */

#ifndef X3PIO_SRC_TIMESTAMP_H
#define X3PIO_SRC_TIMESTAMP_H

#include "common.h"

/**
 * Tells whether the fields of a timestamp are in their ranges and name a day
 * that exists.
 *
 * @param[in] timestamp The timestamp. May be null.
 */
bool x3pio_timestamp_valid(const x3pio_timestamp *timestamp);

#endif /* X3PIO_SRC_TIMESTAMP_H */
