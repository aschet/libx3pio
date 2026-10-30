# libx3pio

[![CI](https://github.com/aschet/libx3pio/actions/workflows/ci.yml/badge.svg)](https://github.com/aschet/libx3pio/actions/workflows/ci.yml)

A portable C library that reads and writes ISO 25178-72 x3p surface texture
files: the revisions of 2017 and of Amendment 1, with the points as binary data
or as text, in application-side buffers with arbitrary strides.

It uses minizip-ng for the zip container and Expat for main.xml, and its
headers include neither.

The [API reference](https://aschet.github.io/libx3pio/) documents the interface
and how to build the library, and the [examples](examples) show how to use it.
