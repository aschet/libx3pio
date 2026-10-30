#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

# Builds the library with the examples, runs the tests and installs the library,
# then builds and runs a consumer project against the installation. This is done
# for the shared library and for the static library. The script reads these
# environment variables:
#   GENERATOR          the CMake generator; a single-configuration generator
#                      builds the Release configuration
#   TOOLCHAIN_OPTIONS  CMake options shared by the library and the consumer
#                      project, such as the compiler and the architecture
#   CONFIGURE_OPTIONS  CMake options for the library only
#   EMULATOR           the command that runs a program built for another
#                      architecture
#   WORKDIR            the directory for the build and the installation; the
#                      current directory if unset
#
# If pkg-config is available, the script also builds a program with the flags
# that pkg-config reports, using the compiler of the build.
set -e

workdir=${WORKDIR:-$PWD}
generator=()
if [ -n "$GENERATOR" ]; then
  generator=(-G "$GENERATOR" -DCMAKE_BUILD_TYPE=Release)
fi

for linkage in ON OFF; do
  echo "== BUILD_SHARED_LIBS=$linkage"
  build="$workdir/build-$linkage"
  stage="$workdir/stage-$linkage"
  # shellcheck disable=SC2086
  cmake -S . -B "$build" "${generator[@]}" $TOOLCHAIN_OPTIONS $CONFIGURE_OPTIONS \
    -DBUILD_SHARED_LIBS=$linkage -DX3PIO_BUILD_EXAMPLES=ON \
    -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
    -DCMAKE_INSTALL_PREFIX="$stage"
  cmake --build "$build" --config Release
  ctest --test-dir "$build" -C Release --output-on-failure --no-tests=error
  cmake --install "$build" --config Release
  # A static library needs the libraries that it was built with. The ones of
  # vcpkg in manifest mode are in the build folder of the library.
  prefix_path="$stage"
  for installed in "$build"/vcpkg_installed/*/; do
    if [ -d "$installed/share" ]; then
      prefix_path="$prefix_path;${installed%/}"
    fi
  done
  # shellcheck disable=SC2086
  cmake -S test_package -B "$build/consumer" "${generator[@]}" \
    $TOOLCHAIN_OPTIONS "-DCMAKE_PREFIX_PATH=$prefix_path"
  cmake --build "$build/consumer" --config Release
  consumer=$(find "$build/consumer" \
    \( -name test_package -o -name test_package.exe \) -type f | head -1)
  # shellcheck disable=SC2086
  PATH="$stage/bin:$PATH" LD_LIBRARY_PATH="$stage/lib" \
    DYLD_LIBRARY_PATH="$stage/lib" $EMULATOR "$consumer"

  pc=$(find "$stage" -name x3pio.pc | head -1)
  if [ -n "$pc" ] && command -v pkg-config > /dev/null; then
    echo "== pkg-config, BUILD_SHARED_LIBS=$linkage"
    compiler=$(sed -n 's/^CMAKE_C_COMPILER:[A-Z]*=//p' "$build/CMakeCache.txt")
    flags=$(sed -n 's/^CMAKE_C_FLAGS:[A-Z]*=//p' "$build/CMakeCache.txt")
    # A static library requires its private dependencies, which pkg-config
    # reports only with --static.
    # The libraries of vcpkg in manifest mode are in the build folder.
    for pkgconfig in "$build"/vcpkg_installed/*/lib/pkgconfig \
      "$build"/vcpkg_installed/*/share/pkgconfig; do
      if [ -d "$pkgconfig" ]; then
        PKG_CONFIG_PATH="$pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
      fi
    done
    export PKG_CONFIG_PATH
    static=""
    if [ "$linkage" = OFF ]; then static="--static"; fi
    # shellcheck disable=SC2046,SC2086
    $compiler $flags test_package/main.c \
      $(PKG_CONFIG_PATH="$(dirname "$pc")${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}" pkg-config $static --cflags --libs \
        x3pio) -o "$build/pkg-config-consumer"
    # shellcheck disable=SC2086
    LD_LIBRARY_PATH="$(dirname "$(dirname "$pc")")" \
      DYLD_LIBRARY_PATH="$(dirname "$(dirname "$pc")")" \
      $EMULATOR "$build/pkg-config-consumer"
  fi
done
