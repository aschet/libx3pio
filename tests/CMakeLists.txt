# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

find_package(Threads)
# libxml2 checks the files that the writer makes against the schemas of the
# standard. The library does not use it.
find_package(LibXml2 REQUIRED)

# A test links the objects of the library when it needs the functions that the
# library hides, and the library when it checks what a user can call.
function(x3pio_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 TEST "" "SOURCE" "")
    if(NOT TEST_SOURCE)
        set(TEST_SOURCE ${name}.c)
    endif()
    add_executable(${name} ${TEST_SOURCE} helpers.c)
    target_link_libraries(${name} PRIVATE x3pio_objects LibXml2::LibXml2)
    target_include_directories(${name} PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_compile_options(${name} PRIVATE ${X3PIO_WARNINGS})
    # The tests link the objects of the library, also when it is shared: its
    # functions are not imported from the DLL.
    target_compile_definitions(${name} PRIVATE
        X3PIO_STATIC_DEFINE
        X3PIO_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
        X3PIO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    )
    add_test(NAME ${name} COMMAND ${name})
    # The suppression of the thread sanitizer, which other builds do not read.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT
        "TSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/tsan.supp")
endfunction()

x3pio_add_test(test_text)
x3pio_add_test(test_md5)
x3pio_add_test(test_byte_order)
x3pio_add_test(test_codec)
x3pio_add_test(test_vendor)
x3pio_add_test(test_document)
x3pio_add_test(test_roundtrip)
x3pio_add_test(test_missing)
x3pio_add_test(test_transformation)
x3pio_add_test(test_samples)
x3pio_add_test(test_tolerance)
x3pio_add_test(test_allocator)
x3pio_add_test(test_files)
# More than 4 GiB of point data, which needs zip64. It runs only if the
# environment variable X3PIO_LARGE_TESTS is set, and is skipped otherwise.
x3pio_add_test(test_large)
set_tests_properties(test_large PROPERTIES SKIP_RETURN_CODE 77)

if(Threads_FOUND AND CMAKE_USE_PTHREADS_INIT)
    x3pio_add_test(test_threads)
    target_link_libraries(test_threads PRIVATE Threads::Threads)
endif()

# The public interface only, through the library that is installed. It uses
# neither the headers nor the libraries of the implementation.
add_executable(test_public test_public.c)
target_link_libraries(test_public PRIVATE x3pio)
target_compile_options(test_public PRIVATE ${X3PIO_WARNINGS})
add_test(NAME test_public COMMAND test_public)

# No public header names a library of the implementation, and every public header
# compiles by itself, in C and in C++.
add_executable(test_headers test_headers.c)
target_compile_definitions(test_headers PRIVATE
    X3PIO_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
)
target_compile_options(test_headers PRIVATE ${X3PIO_WARNINGS})
add_test(NAME test_headers COMMAND test_headers)
