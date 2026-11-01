# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

# Finds the libraries that libx3pio is built with and sets
# X3PIO_MINIZIP_TARGET and X3PIO_EXPAT_TARGET to their imported targets. The
# package managers name them differently:
#
#   minizip-ng   vcpkg, upstream   package minizip-ng   MINIZIP::minizip-ng
#                Conan Center      package minizip      MINIZIP::minizip
#   Expat        CMake, system     module EXPAT         EXPAT::EXPAT
#                vcpkg, Conan      package expat        expat::expat
#
# minizip-ng installs the headers in an include/minizip-ng folder, which the
# source includes as <mz.h>, as minizip-ng does itself. The target of Conan only
# adds include, so that folder is added here.
#
# The targets are put behind two interface targets of fixed names,
# x3pio_deps::minizip and x3pio_deps::expat, which the package of a static
# library refers to. So the package works for a program that finds the libraries
# under another name than the one the library was built with.
#
# The macro is included by the build of the library and by the package
# configuration of a static library, so that both look for the libraries the
# same way.

macro(x3pio_find_dependencies)
    # The libraries that a package manager provides come before those of the
    # system: a program that names them in a toolchain file or in a prefix
    # path wants those, also where the system has a package of its own.
    set(_x3pio_explicit NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_SYSTEM_PATH
        NO_CMAKE_SYSTEM_PACKAGE_REGISTRY)
    find_package(minizip-ng CONFIG QUIET ${_x3pio_explicit})
    if(NOT TARGET MINIZIP::minizip-ng)
        find_package(minizip CONFIG QUIET ${_x3pio_explicit})
    endif()
    if(NOT TARGET MINIZIP::minizip-ng AND NOT TARGET MINIZIP::minizip)
        find_package(minizip-ng CONFIG QUIET)
    endif()
    if(NOT TARGET MINIZIP::minizip-ng AND NOT TARGET MINIZIP::minizip)
        find_package(minizip CONFIG REQUIRED)
    endif()
    if(TARGET MINIZIP::minizip-ng)
        set(_x3pio_minizip MINIZIP::minizip-ng)
    else()
        set(_x3pio_minizip MINIZIP::minizip)
        get_target_property(_x3pio_mz_dirs ${_x3pio_minizip} INTERFACE_INCLUDE_DIRECTORIES)
        foreach(_x3pio_mz_dir IN LISTS _x3pio_mz_dirs)
            if(EXISTS "${_x3pio_mz_dir}/minizip-ng/mz.h" AND NOT EXISTS "${_x3pio_mz_dir}/mz.h")
                target_include_directories(${_x3pio_minizip} INTERFACE "${_x3pio_mz_dir}/minizip-ng")
            endif()
        endforeach()
    endif()

    find_package(EXPAT REQUIRED)
    if(TARGET EXPAT::EXPAT)
        set(_x3pio_expat EXPAT::EXPAT)
    elseif(TARGET expat::expat)
        set(_x3pio_expat expat::expat)
    else()
        message(FATAL_ERROR "Expat was found, but not as the target EXPAT::EXPAT or expat::expat")
    endif()

    if(NOT TARGET x3pio_deps::minizip)
        add_library(x3pio_deps::minizip INTERFACE IMPORTED)
        target_link_libraries(x3pio_deps::minizip INTERFACE ${_x3pio_minizip})
    endif()
    if(NOT TARGET x3pio_deps::expat)
        add_library(x3pio_deps::expat INTERFACE IMPORTED)
        target_link_libraries(x3pio_deps::expat INTERFACE ${_x3pio_expat})
    endif()
    set(X3PIO_MINIZIP_TARGET x3pio_deps::minizip)
    set(X3PIO_EXPAT_TARGET x3pio_deps::expat)
endmacro()
