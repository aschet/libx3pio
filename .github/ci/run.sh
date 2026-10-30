#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

# Runs a job of the continuous integration. GitHub and a developer machine run
# the jobs with this same script.
#
#   run.sh <job>          runs the job in build/<job>
#   run.sh --list         prints the names of the jobs
#
# A build job builds the library as a static and as a shared library, runs the
# tests, installs the library, and builds and runs a consumer project against
# the installation. The libraries minizip-ng, Expat and libxml2 come from vcpkg
# in manifest mode, with the root that VCPKG_ROOT or, on the runners of GitHub,
# VCPKG_INSTALLATION_ROOT names. DEPENDENCY_OPTIONS replaces that with other CMake
# options, for example the toolchain file of Conan.
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
root=$(cd "$here/../.." && pwd)

jobs=(
  gcc-x86-64-linux
  gcc-x86-linux
  gcc-aarch64-linux
  clang-x86-64-linux
  clang-aarch64-macos
  msvc-x86-64-windows
  msvc-x86-windows
  msvc-aarch64-windows
  asan-ubsan
  tsan
  static-analysis
  format
  documentation
  licenses
  coverage
)

# Describes a job by setting variables. kind is build or check.
# A build job sets:
#   generator  the CMake generator; empty selects the default of the platform
#   toolchain  CMake options shared by the library and the consumer project
#   configure  CMake options for the library only
#   triplet    the triplet of vcpkg
kind=
generator=
toolchain=
configure=
triplet=

describe() {
  case "$1" in
    # Strict conformance: ISO C11 without compiler extensions, with every
    # diagnostic that -pedantic-errors reports treated as an error.
    gcc-x86-64-linux)
      kind=build generator=Ninja triplet=x64-linux
      toolchain="-DCMAKE_C_COMPILER=gcc"
      configure="-DCMAKE_C_STANDARD=11 -DCMAKE_C_STANDARD_REQUIRED=ON -DCMAKE_C_FLAGS=-pedantic-errors"
      ;;
    gcc-x86-linux)
      kind=build generator=Ninja triplet=x86-linux
      toolchain="-DCMAKE_C_COMPILER=gcc -DCMAKE_C_FLAGS=-m32 -DCMAKE_EXE_LINKER_FLAGS=-m32 -DCMAKE_SHARED_LINKER_FLAGS=-m32"
      ;;
    gcc-aarch64-linux)
      kind=build generator=Ninja triplet=arm64-linux
      toolchain="-DCMAKE_C_COMPILER=gcc"
      ;;
    clang-x86-64-linux)
      kind=build generator=Ninja triplet=x64-linux
      toolchain="-DCMAKE_C_COMPILER=clang"
      ;;
    clang-aarch64-macos)
      kind=build generator=Ninja triplet=arm64-osx
      toolchain="-DCMAKE_C_COMPILER=clang"
      ;;
    msvc-x86-64-windows)
      kind=build triplet=x64-windows toolchain="-A x64"
      ;;
    msvc-x86-windows)
      kind=build triplet=x86-windows toolchain="-A Win32"
      ;;
    msvc-aarch64-windows)
      kind=build triplet=arm64-windows toolchain="-A ARM64"
      ;;
    asan-ubsan | tsan | static-analysis | format | documentation | licenses \
      | coverage)
      kind=check
      ;;
    *)
      echo "Unknown job: $1 (run.sh --list names them)" >&2
      exit 2
      ;;
  esac
}

# Prints the CMake options that make the dependencies findable.
dependency_options() {
  if [ -n "${DEPENDENCY_OPTIONS:-}" ]; then
    echo "$DEPENDENCY_OPTIONS"
    return
  fi
  local vcpkg=${VCPKG_ROOT:-${VCPKG_INSTALLATION_ROOT:-}}
  if [ -z "$vcpkg" ]; then
    echo "Set VCPKG_ROOT, or DEPENDENCY_OPTIONS for another source of the dependencies." >&2
    exit 1
  fi
  local options="-DCMAKE_TOOLCHAIN_FILE=$vcpkg/scripts/buildsystems/vcpkg.cmake"
  options="$options -DVCPKG_MANIFEST_FEATURES=tests"
  if [ -n "${1:-}" ]; then
    options="$options -DVCPKG_TARGET_TRIPLET=$1"
  fi
  echo "$options"
}

run_build() {
  GENERATOR=$generator \
    TOOLCHAIN_OPTIONS="$toolchain $(dependency_options "$triplet")" \
    CONFIGURE_OPTIONS=$configure WORKDIR="$dir" \
    "$here/install-and-use.sh"
}

# ThreadSanitizer cannot be combined with AddressSanitizer, so each runs in a
# build of its own. The argument holds the flags of the compiler and the linker.
run_sanitizer() {
  if [ -n "${CI:-}" ]; then
    # Lowers the entropy of address-space layout randomization. At the default
    # setting of the runners, the sanitizers cannot map their shadow memory.
    sudo sysctl vm.mmap_rnd_bits=28 || true
  fi
  # shellcheck disable=SC2046
  CC=clang cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    "-DCMAKE_C_FLAGS=$1" "-DCMAKE_EXE_LINKER_FLAGS=$1" \
    "-DCMAKE_SHARED_LINKER_FLAGS=$1" -DCMAKE_COMPILE_WARNING_AS_ERROR=ON \
    -DX3PIO_BUILD_EXAMPLES=ON $(dependency_options "")
  cmake --build "$dir"
  ctest --test-dir "$dir" --output-on-failure --no-tests=error
}

run_static_analysis() {
  # shellcheck disable=SC2046
  cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DX3PIO_BUILD_EXAMPLES=ON \
    $(dependency_options "")
  # shellcheck disable=SC2046
  clang-tidy -p "$dir" --warnings-as-errors='*' \
    $(ls src/*.c tests/*.c examples/*.c test_package/*.c)
}

run_format() {
  clang-format --dry-run -Werror src/*.c src/*.h include/x3pio/*.h \
    tests/*.c test_package/*.c examples/*.c
}

run_documentation() {
  # shellcheck disable=SC2046
  cmake -S . -B "$dir" -G Ninja -DX3PIO_BUILD_DOCS=ON -DBUILD_TESTING=OFF \
    $(dependency_options "")
  mkdir -p "$dir"
  cmake --build "$dir" --target docs 2>&1 | tee "$dir/docs.log"
  if grep -i "warning" "$dir/docs.log"; then
    exit 1
  fi
}

run_licenses() {
  reuse lint
}

# Measures the coverage of the library sources by the tests, excluding the
# tests and the examples. The tests use threads, so the profiling counters are
# updated atomically. Some tests run a second build of the library, so a
# function can have two positions, and the first one counts. The job fails if
# fewer than 75 percent of the lines are executed.
run_coverage() {
  # The data of an earlier build, also for source files that do not exist any
  # more, would be read as well.
  rm -rf "$dir"
  # shellcheck disable=SC2046
  cmake -S . -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=gcc "-DCMAKE_C_FLAGS=--coverage -fprofile-update=atomic" \
    $(dependency_options "")
  cmake --build "$dir"
  ctest --test-dir "$dir" --output-on-failure --no-tests=error
  gcovr --root . --filter 'src/' --exclude-throw-branches \
    --merge-mode-functions=merge-use-line-min --fail-under-line 75 \
    --print-summary --txt "$dir/coverage.txt" "$dir"
}

main() {
  case "${1:-}" in
    --list)
      printf '%s\n' "${jobs[@]}"
      ;;
    "" | -*)
      echo "Usage: run.sh <job> | --list" >&2
      exit 2
      ;;
    *)
      describe "$1"
      cd "$root"
      dir="$root/build/$1"
      case "$1" in
        asan-ubsan)
          run_sanitizer "-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer"
          ;;
        tsan) run_sanitizer "-fsanitize=thread -fno-omit-frame-pointer" ;;
        static-analysis) run_static_analysis ;;
        format) run_format ;;
        documentation) run_documentation ;;
        licenses) run_licenses ;;
        coverage) run_coverage ;;
        *) "run_$kind" ;;
      esac
      ;;
  esac
}

main "$@"
