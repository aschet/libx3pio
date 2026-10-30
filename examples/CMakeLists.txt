# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

foreach(example write_heights read_heights read_stream write_data read_data
        points layers vendor_files allocators write_memory)
    add_executable(${example} ${example}.c)
    target_link_libraries(${example} PRIVATE x3pio)
    target_compile_options(${example} PRIVATE ${X3PIO_WARNINGS})
    target_compile_definitions(${example} PRIVATE
        $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    )
endforeach()

# The examples run as tests, in the order in which one makes the files that the
# next one reads.
if(BUILD_TESTING)
    set(X3PIO_EXAMPLE_DIR "${CMAKE_CURRENT_BINARY_DIR}/run")
    file(MAKE_DIRECTORY "${X3PIO_EXAMPLE_DIR}")
    function(x3pio_example_test name)
        cmake_parse_arguments(PARSE_ARGV 1 EXAMPLE "" "PROGRAM;INPUT" "ARGS;REQUIRES;PROVIDES")
        add_test(NAME example_${name}
                 COMMAND ${EXAMPLE_PROGRAM} ${EXAMPLE_ARGS}
                 WORKING_DIRECTORY "${X3PIO_EXAMPLE_DIR}")
        if(EXAMPLE_REQUIRES)
            set_tests_properties(example_${name} PROPERTIES FIXTURES_REQUIRED "${EXAMPLE_REQUIRES}")
        endif()
        if(EXAMPLE_PROVIDES)
            set_tests_properties(example_${name} PROPERTIES FIXTURES_SETUP "${EXAMPLE_PROVIDES}")
        endif()
    endfunction()

    x3pio_example_test(write_heights PROGRAM write_heights ARGS surface PROVIDES surfaces)
    x3pio_example_test(read_heights PROGRAM read_heights ARGS surface_float.x3p REQUIRES surfaces)
    x3pio_example_test(read_heights_int16 PROGRAM read_heights ARGS surface_int16.x3p REQUIRES surfaces)
    x3pio_example_test(read_heights_2017 PROGRAM read_heights ARGS surface_2017.x3p REQUIRES surfaces)
    x3pio_example_test(allocators PROGRAM allocators ARGS surface_float.x3p REQUIRES surfaces)
    x3pio_example_test(write_data PROGRAM write_data ARGS counts.x3p PROVIDES counts)
    x3pio_example_test(read_data PROGRAM read_data ARGS counts.x3p REQUIRES counts)
    x3pio_example_test(points PROGRAM points ARGS cloud.x3p)
    x3pio_example_test(layers PROGRAM layers ARGS layers.x3p)
    x3pio_example_test(vendor_files PROGRAM vendor_files ARGS vendor.x3p)
    x3pio_example_test(write_memory PROGRAM write_memory)
    add_test(NAME example_read_stream
             COMMAND ${CMAKE_COMMAND} -DPROGRAM=$<TARGET_FILE:read_stream>
                     -DINPUT=${X3PIO_EXAMPLE_DIR}/surface_float.x3p
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/pipe.cmake
             WORKING_DIRECTORY "${X3PIO_EXAMPLE_DIR}")
    set_tests_properties(example_read_stream PROPERTIES FIXTURES_REQUIRED surfaces)
endif()
