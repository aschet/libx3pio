# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.28...4.2)

project(x3pio
    VERSION 0.1.0
    DESCRIPTION "Reader and writer for ISO 25178-72 x3p files in C"
    HOMEPAGE_URL "https://github.com/aschet/libx3pio"
    LANGUAGES C
)

# Standard C for every target: no extensions of the compiler. The standard
# itself is the default of the compiler, but at least C11, which the library
# asks for with a compile feature.
set(CMAKE_C_EXTENSIONS OFF)

include(CMakePackageConfigHelpers)
include(CTest)
include(GenerateExportHeader)
include(GNUInstallDirs)

option(X3PIO_BUILD_EXAMPLES "Build the examples" OFF)
option(X3PIO_BUILD_EXTERNAL_EXAMPLES "Build the examples that use OpenCV, VTK and Eigen, which must be found" OFF)
option(X3PIO_BUILD_DOCS "Build the API reference with Doxygen" OFF)

set(X3PIO_WARNINGS
    $<$<C_COMPILER_ID:MSVC>:/W4;/permissive-;/utf-8>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion;-Wstrict-prototypes;-Wmissing-prototypes;-Wcast-qual;-Wformat=2;-Wundef>
)

# A Windows program finds the DLL beside itself: the tests, the examples and the
# library go to one folder.
if(WIN32 AND NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
endif()

# The libraries of the zip container and of the XML of main.xml. They are
# details of the implementation: no public header includes one of them.
include(cmake/x3pio-find-dependencies.cmake)
x3pio_find_dependencies()

set(X3PIO_GENERATED_INCLUDE_DIR "${PROJECT_BINARY_DIR}/generated/include")

set(X3PIO_SOURCES
    src/allocator.c
    src/allocator.h
    src/buffer.c
    src/buffer.h
    src/byte_order.c
    src/byte_order.h
    src/data_type.c
    src/data_type.h
    src/matrix.c
    src/matrix.h
    src/samples.c
    src/samples.h
    src/common.h
    src/document.c
    src/document_check.c
    src/document_feature.c
    src/document_metadata.c
    src/document.h
    src/convert.c
    src/layout.c
    src/md5.c
    src/md5.h
    src/members.c
    src/members.h
    src/memory_stream.c
    src/memory_stream.h
    src/minizip.h
    src/reader.c
    src/reader_binary.c
    src/reader_main.c
    src/reader_vendor.c
    src/reader.h
    src/reader_points.c
    src/text.c
    src/text.h
    src/timestamp.c
    src/timestamp.h
    src/types.c
    src/vendor.c
    src/vendor.h
    src/writer.c
    src/writer_end.c
    src/writer_vendor.c
    src/writer.h
    src/writer_points.c
    src/xml.h
    src/expat_memory.c
    src/expat_memory.h
    src/xml_build.c
    src/xml_read.c
    src/xml_scan.c
    src/xml_state.h
    src/xml_write.c
    src/zip.h
    src/zip_read.c
    src/zip_write.c
)

# The files by wide-character path exist on Windows only.
if(WIN32)
    list(APPEND X3PIO_SOURCES src/file_win32.c)
endif()

# The code is compiled once. The tests link the objects, which have the
# internal functions that the library hides.
add_library(x3pio_objects OBJECT ${X3PIO_SOURCES})
target_include_directories(x3pio_objects
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
        "${X3PIO_GENERATED_INCLUDE_DIR}"
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_compile_features(x3pio_objects PUBLIC c_std_11)
target_compile_options(x3pio_objects PRIVATE ${X3PIO_WARNINGS})
# The library is standard C11, with no feature macros but those that a source
# file names itself.
target_compile_definitions(x3pio_objects PRIVATE
    $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
)
target_link_libraries(x3pio_objects PUBLIC
    ${X3PIO_MINIZIP_TARGET}
    ${X3PIO_EXPAT_TARGET}
    $<$<NOT:$<PLATFORM_ID:Windows>>:m>
)
set_target_properties(x3pio_objects PROPERTIES
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
)

add_library(x3pio $<TARGET_OBJECTS:x3pio_objects>)
target_compile_features(x3pio PUBLIC c_std_11)
add_library(x3pio::x3pio ALIAS x3pio)

generate_export_header(x3pio
    BASE_NAME x3pio
    EXPORT_MACRO_NAME X3PIO_API
    EXPORT_FILE_NAME "${X3PIO_GENERATED_INCLUDE_DIR}/x3pio/x3pio_export.h"
)
configure_file(include/x3pio/version.h.in
    "${X3PIO_GENERATED_INCLUDE_DIR}/x3pio/version.h" @ONLY
)

# The objects are compiled for the library, so they export what it exports.
target_compile_definitions(x3pio_objects PRIVATE x3pio_EXPORTS)
if(NOT BUILD_SHARED_LIBS)
    target_compile_definitions(x3pio_objects PUBLIC X3PIO_STATIC_DEFINE)
    target_compile_definitions(x3pio PUBLIC X3PIO_STATIC_DEFINE)
endif()

target_sources(x3pio
    PUBLIC
        FILE_SET HEADERS
            BASE_DIRS
                "${PROJECT_SOURCE_DIR}/include"
                "${X3PIO_GENERATED_INCLUDE_DIR}"
            FILES
                include/x3pio/convert.h
                include/x3pio/document.h
                include/x3pio/file.h
                include/x3pio/layout.h
                include/x3pio/reader.h
                include/x3pio/types.h
                include/x3pio/writer.h
                include/x3pio/x3pio.h
                "${X3PIO_GENERATED_INCLUDE_DIR}/x3pio/x3pio_export.h"
                "${X3PIO_GENERATED_INCLUDE_DIR}/x3pio/version.h"
)
target_link_libraries(x3pio
    PUBLIC
        $<$<NOT:$<PLATFORM_ID:Windows>>:m>
    PRIVATE
        ${X3PIO_MINIZIP_TARGET}
        ${X3PIO_EXPAT_TARGET}
)

# A shared library must not re-export the symbols of the static libraries that
# are linked into it, such as zlib: they would clash with the copies of a
# program that links the same library itself.
if(BUILD_SHARED_LIBS AND CMAKE_SYSTEM_NAME MATCHES "Linux|FreeBSD|NetBSD|OpenBSD|SunOS")
    target_link_options(x3pio PRIVATE "LINKER:--exclude-libs,ALL")
endif()

set_target_properties(x3pio PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME x3pio
    C_EXTENSIONS OFF
    C_VISIBILITY_PRESET hidden
    POSITION_INDEPENDENT_CODE ON
)

if(X3PIO_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(X3PIO_BUILD_EXTERNAL_EXAMPLES)
    enable_language(CXX)
    add_subdirectory(examples/external)
endif()

if(X3PIO_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    set(X3PIO_DOXYGEN_LAYOUT "${PROJECT_BINARY_DIR}/DoxygenLayout.xml")
    execute_process(
        COMMAND "${DOXYGEN_EXECUTABLE}" -l "${X3PIO_DOXYGEN_LAYOUT}"
        RESULT_VARIABLE layout_result
        OUTPUT_QUIET
        ERROR_QUIET
    )
    file(READ "${X3PIO_DOXYGEN_LAYOUT}" layout)
    set(pages_tab "<tab type=\"pages\" visible=\"yes\" title=\"\" intro=\"\"/>")
    string(FIND "${layout}" "${pages_tab}" pages_tab_at)
    if(NOT layout_result EQUAL 0 OR pages_tab_at EQUAL -1)
        message(FATAL_ERROR "The layout of Doxygen has no tab for the pages")
    endif()
    string(REPLACE "${pages_tab}"
        "<tab type=\"user\" url=\"@ref format\" title=\"x3p Format\" intro=\"\"/>"
        layout "${layout}")
    file(WRITE "${X3PIO_DOXYGEN_LAYOUT}" "${layout}")
    configure_file(cmake/Doxyfile.in "${PROJECT_BINARY_DIR}/Doxyfile" @ONLY)
    add_custom_target(docs
        COMMAND "${DOXYGEN_EXECUTABLE}" "${PROJECT_BINARY_DIR}/Doxyfile"
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
        COMMENT "Generating the API reference"
        VERBATIM
    )
endif()

if(PROJECT_IS_TOP_LEVEL AND BUILD_TESTING)
    add_subdirectory(tests)
endif()

set(x3pio_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/x3pio")

install(TARGETS x3pio
    EXPORT x3pio-targets
    FILE_SET HEADERS
)

install(EXPORT x3pio-targets
    FILE x3pio-targets.cmake
    NAMESPACE x3pio::
    DESTINATION "${x3pio_cmakedir}"
)

write_basic_package_version_file("${PROJECT_BINARY_DIR}/x3pio-config-version.cmake"
    COMPATIBILITY SameMajorVersion
)
configure_package_config_file(cmake/x3pio-config.cmake.in
    "${PROJECT_BINARY_DIR}/x3pio-config.cmake"
    INSTALL_DESTINATION "${x3pio_cmakedir}"
)
install(FILES
    "${PROJECT_BINARY_DIR}/x3pio-config.cmake"
    "${PROJECT_BINARY_DIR}/x3pio-config-version.cmake"
    cmake/x3pio-find-dependencies.cmake
    DESTINATION "${x3pio_cmakedir}"
)
install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/licenses/x3pio")

# For programs that find libraries with pkg-config. A static library needs the
# macro that tells the headers that it is not a DLL, and the libraries that it
# was built with.
if(UNIX)
    set(X3PIO_PC_CFLAGS "")
    set(X3PIO_PC_REQUIRES "")
    set(X3PIO_PC_LIBS_PRIVATE "")
    if(NOT BUILD_SHARED_LIBS)
        set(X3PIO_PC_CFLAGS " -DX3PIO_STATIC_DEFINE")
        set(X3PIO_PC_REQUIRES "expat minizip-ng")
        if(APPLE)
            # minizip-ng compresses with the Compression library of macOS
            # there, which its pkg-config file does not name.
            set(X3PIO_PC_LIBS_PRIVATE " -lcompression")
        endif()
    endif()
    configure_file(cmake/x3pio.pc.in "${PROJECT_BINARY_DIR}/x3pio.pc" @ONLY)
    install(FILES "${PROJECT_BINARY_DIR}/x3pio.pc"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()
