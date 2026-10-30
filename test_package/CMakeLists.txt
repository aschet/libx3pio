# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.28)
project(x3pio_test_package LANGUAGES C)

set(CMAKE_C_EXTENSIONS OFF)

find_package(x3pio REQUIRED)

add_executable(test_package main.c)
target_link_libraries(test_package PRIVATE x3pio::x3pio)
