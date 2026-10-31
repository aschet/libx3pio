#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: 0BSD

"""Write and read an x3p file from Python with ctypes, the module of the
standard library that calls C functions. Nothing but the shared library
libx3pio is needed.

    surface.py [path/to/libx3pio.so] [file.x3p]

The library is also found through the environment variable X3PIO_LIBRARY, or
by name if it is installed. The program writes a surface of 64 by 48 heights
as floats, reads it back, and prints what it finds.
"""

import ctypes
import math
import os
import sys
from array import array
from ctypes import (
    POINTER,
    Structure,
    byref,
    c_bool,
    c_char_p,
    c_double,
    c_int,
    c_size_t,
    c_ssize_t,
    c_void_p,
)
from ctypes.util import find_library

# The values of the enumerations of <x3pio/types.h>.
X3PIO_OK = 0
X3PIO_TYPE_FLOAT = 6
X3PIO_TYPE_DOUBLE = 7


class Layout(Structure):
    """x3pio_layout: how a matrix of numbers lies in memory."""

    _fields_ = [
        ("type", c_int),
        ("element_stride", c_ssize_t),
        ("row_stride", c_ssize_t),
    ]


class Dimensions(Structure):
    """x3pio_dimensions: the number of points."""

    _fields_ = [
        ("size_x", c_size_t),
        ("size_y", c_size_t),
        ("size_z", c_size_t),
        ("list", c_bool),
    ]


def load_library(argument):
    """Loads the shared library and declares the functions that are used."""
    candidates = [argument, os.environ.get("X3PIO_LIBRARY"), find_library("x3pio")]
    path = next((name for name in candidates if name), None)
    if path is None:
        sys.exit("libx3pio was not found: give its path or set X3PIO_LIBRARY")
    lib = ctypes.CDLL(path)

    def declare(name, restype, *argtypes):
        function = getattr(lib, name)
        function.restype = restype
        function.argtypes = argtypes

    handle = c_void_p
    declare("x3pio_document_create", c_int, POINTER(handle))
    declare("x3pio_document_free", None, handle)
    declare(
        "x3pio_document_set_surface",
        c_int,
        handle,
        c_size_t,
        c_size_t,
        c_double,
        c_double,
        c_int,
        c_double,
        c_size_t,
    )
    declare("x3pio_document_get_dimensions", c_int, handle, POINTER(Dimensions))
    declare("x3pio_layout_packed", POINTER(Layout), c_int)
    declare("x3pio_writer_create", c_int, POINTER(handle))
    declare("x3pio_writer_free", None, handle)
    declare("x3pio_create_file", c_int, handle, c_char_p, handle)
    declare("x3pio_write_heights", c_int, handle, POINTER(Layout), c_void_p)
    declare("x3pio_end_file", c_int, handle)
    declare("x3pio_writer_error_message", c_char_p, handle)
    declare("x3pio_reader_create", c_int, POINTER(handle))
    declare("x3pio_reader_free", None, handle)
    declare("x3pio_open_file", c_int, handle, c_char_p)
    declare("x3pio_reader_document", handle, handle)
    declare("x3pio_read_heights", c_int, handle, POINTER(Layout), c_void_p)
    declare("x3pio_reader_error_message", c_char_p, handle)
    return lib


def check(status, message=b""):
    if status != X3PIO_OK:
        text = message.decode() if message else f"status {status}"
        raise RuntimeError(text)


def write_surface(lib, path, columns, rows, heights):
    """Writes the heights, a row after the other, in metres."""
    document = c_void_p()
    check(lib.x3pio_document_create(byref(document)))
    writer = c_void_p()
    try:
        # A surface of floats: the points lie 1 micrometre apart.
        check(
            lib.x3pio_document_set_surface(
                document, columns, rows, 1e-6, 1e-6, X3PIO_TYPE_FLOAT, 1.0, 1
            )
        )
        check(lib.x3pio_writer_create(byref(writer)))
        check(
            lib.x3pio_create_file(writer, path.encode(), document),
            lib.x3pio_writer_error_message(writer),
        )
        # The buffer is an array of floats that follow each other.
        address, _ = heights.buffer_info()
        layout = lib.x3pio_layout_packed(X3PIO_TYPE_FLOAT)
        check(
            lib.x3pio_write_heights(writer, layout, address),
            lib.x3pio_writer_error_message(writer),
        )
        check(lib.x3pio_end_file(writer), lib.x3pio_writer_error_message(writer))
    finally:
        if writer:
            lib.x3pio_writer_free(writer)
        lib.x3pio_document_free(document)


def read_surface(lib, path):
    """Returns the columns, the rows and the heights as an array of doubles."""
    reader = c_void_p()
    check(lib.x3pio_reader_create(byref(reader)))
    try:
        check(
            lib.x3pio_open_file(reader, path.encode()),
            lib.x3pio_reader_error_message(reader),
        )
        # The description is available when the file is open: the program
        # allocates the buffer and the library fills it.
        dimensions = Dimensions()
        document = lib.x3pio_reader_document(reader)
        check(lib.x3pio_document_get_dimensions(document, byref(dimensions)))
        count = dimensions.size_x * dimensions.size_y * dimensions.size_z
        heights = array("d", bytes(8 * count))
        address, _ = heights.buffer_info()
        layout = lib.x3pio_layout_packed(X3PIO_TYPE_DOUBLE)
        check(
            lib.x3pio_read_heights(reader, layout, address),
            lib.x3pio_reader_error_message(reader),
        )
        return dimensions.size_x, dimensions.size_y, heights
    finally:
        lib.x3pio_reader_free(reader)


def main(arguments):
    lib = load_library(arguments[0] if arguments else None)
    path = arguments[1] if len(arguments) > 1 else "surface_python.x3p"
    columns, rows = 64, 48
    heights = array(
        "f",
        (
            1e-6 * math.sin(0.2 * x) * math.cos(0.15 * y)
            for y in range(rows)
            for x in range(columns)
        ),
    )
    write_surface(lib, path, columns, rows, heights)
    read_columns, read_rows, read = read_surface(lib, path)
    print(f"{read_columns} x {read_rows} points, z from {min(read):g} m to {max(read):g} m")
    same = read_columns == columns and read_rows == rows and list(read) == list(heights)
    print("the heights are the ones that were written" if same else "the heights differ")
    return 0 if same else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
