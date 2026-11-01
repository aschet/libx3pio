#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

"""Runs a job of the continuous integration.

The workflows of GitHub call this script for each job, and the same command runs
the job on a development machine.

    ci.py JOB     runs the job in build/JOB
    ci.py --list  prints the names of the jobs

A build job builds the library as a static and as a shared library, runs the
tests, installs the library, and builds and runs a consumer project against the
installation.

The libraries minizip-ng, Expat and libxml2 come from vcpkg in manifest mode,
from the root that VCPKG_ROOT or, on the runners of GitHub,
VCPKG_INSTALLATION_ROOT names. The environment variable DEPENDENCY_OPTIONS
replaces that with other CMake options, for example the toolchain file of Conan.
"""

import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
from functools import partial
from pathlib import Path
from typing import NamedTuple

ROOT = Path(__file__).resolve().parents[2]
NINJA = "Ninja"


class Build(NamedTuple):
    """The properties of a build job. The generator, the platform of a Visual
    Studio generator, the compiler and the flags apply to the library and to the
    consumer project. The triplet is that of vcpkg. The machine is the
    architecture that the job requires natively. A strict job builds the library
    as ISO C11 without extensions, with every diagnostic of -pedantic-errors
    treated as an error."""

    triplet: str
    generator: str = ""
    platform: str = ""
    compiler: str = ""
    flags: str = ""
    machine: str = ""
    strict: bool = False
    external_examples: bool = False


BUILDS = {
    "gcc-x86-64-linux": Build(
        "x64-linux",
        generator=NINJA,
        compiler="gcc",
        machine="x86_64",
        strict=True,
        external_examples=True,
    ),
    "gcc-x86-linux": Build("x86-linux", generator=NINJA, compiler="gcc", flags="-m32"),
    "gcc-aarch64-linux": Build(
        "arm64-linux", generator=NINJA, compiler="gcc", machine="aarch64"
    ),
    "clang-x86-64-linux": Build(
        "x64-linux", generator=NINJA, compiler="clang", machine="x86_64"
    ),
    "clang-aarch64-macos": Build("arm64-osx", generator=NINJA, compiler="clang"),
    "msvc-x86-64-windows": Build("x64-windows", platform="x64"),
    "msvc-x86-windows": Build("x86-windows", platform="Win32"),
    "msvc-aarch64-windows": Build("arm64-windows", platform="ARM64"),
}


def run(command, *arguments, env=None):
    """Runs a command in the root directory and ends the script if it fails. The
    command is split into words as a shell does; the arguments are passed as
    they are, so that they can be paths."""
    command = [*shlex.split(command), *map(str, arguments)]
    print("+", shlex.join(command), flush=True)
    try:
        code = subprocess.run(command, cwd=ROOT, env=env).returncode
    except FileNotFoundError:
        sys.exit(f"The command {command[0]} was not found.")
    if code != 0:
        sys.exit(code)


def machine_is(arch):
    """Whether the machine has the architecture: x86_64, aarch64, arm, riscv64
    or s390x."""
    machine = platform.machine().lower()
    if machine.startswith("armv"):
        machine = "arm"
    return {"arm64": "aarch64", "amd64": "x86_64"}.get(machine, machine) == arch


def dependency_options(triplet=""):
    """The CMake options that make the dependencies findable."""
    options = os.environ.get("DEPENDENCY_OPTIONS")
    if options:
        return shlex.split(options)
    root = os.environ.get("VCPKG_ROOT") or os.environ.get("VCPKG_INSTALLATION_ROOT")
    if not root:
        sys.exit(
            "Set VCPKG_ROOT, or DEPENDENCY_OPTIONS for another source of the "
            "dependencies."
        )
    toolchain = Path(root) / "scripts" / "buildsystems" / "vcpkg.cmake"
    options = [f"-DCMAKE_TOOLCHAIN_FILE={toolchain}", "-DVCPKG_MANIFEST_FEATURES=tests"]
    if triplet:
        options.append(f"-DVCPKG_TARGET_TRIPLET={triplet}")
    return options


def conan_generators():
    """The folder of the files that conan install made, found by the toolchain
    file of the dependency options, or None if the dependencies come from
    elsewhere."""
    for option in dependency_options():
        name, _, value = option.partition("=")
        if name == "-DCMAKE_TOOLCHAIN_FILE" and Path(value).name == "conan_toolchain.cmake":
            return Path(value).parent
    return None


def vcpkg_installed(build):
    """The folders of the libraries that vcpkg installed in manifest mode into
    the build folder of the library."""
    return sorted(path for path in build.glob("vcpkg_installed/*") if path.is_dir())


def library_env(library_dir, path_dir=None):
    """The environment that lets a program find the shared library: in the
    directories of the library path, and on Windows in the path of the programs."""
    env = dict(os.environ)
    env["LD_LIBRARY_PATH"] = env["DYLD_LIBRARY_PATH"] = str(library_dir)
    if path_dir:
        env["PATH"] = f"{path_dir}{os.pathsep}{env['PATH']}"
    return env


def find(directory, *names):
    """The first file of one of the names under a directory."""
    files = (path for path in sorted(directory.rglob("*")) if path.is_file())
    return next((path for path in files if path.name in names), None)


def cache_value(build, name):
    """The value of a variable in the CMake cache of a build."""
    cache = (build / "CMakeCache.txt").read_text()
    match = re.search(rf"^{name}:[A-Z]*=(.*)$", cache, re.MULTILINE)
    return match.group(1) if match else ""


def check_pkg_config(build, pc, static):
    """Builds and runs a program with the flags that pkg-config reports, using
    the compiler of the build."""
    # A static library requires its private dependencies, which pkg-config
    # reports only with --static.
    query = f"pkg-config {'--static' if static else ''} --cflags --libs x3pio"
    # The libraries of vcpkg are in the build folder, and those of Conan have
    # their pc files next to the toolchain file (PkgConfigDeps).
    paths = [pc.parent]
    for installed in vcpkg_installed(build):
        paths += [installed / "lib" / "pkgconfig", installed / "share" / "pkgconfig"]
    generators = conan_generators()
    if generators is not None:
        paths.append(generators)
    paths.append(os.environ.get("PKG_CONFIG_PATH", ""))
    env = dict(os.environ, PKG_CONFIG_PATH=os.pathsep.join(map(str, paths)))
    flags = subprocess.check_output(shlex.split(query), env=env, text=True)
    program = build / "pkg-config-consumer"
    compiler_flags = cache_value(build, "CMAKE_C_FLAGS")
    run(
        cache_value(build, "CMAKE_C_COMPILER"),
        *shlex.split(f"{compiler_flags} test_package/main.c {flags} -o"),
        program,
    )
    run(shlex.quote(str(program)), env=library_env(pc.parent.parent))


def build_job(job, directory):
    properties = BUILDS[job]
    if properties.machine and not machine_is(properties.machine):
        sys.exit(f"This job needs the architecture {properties.machine}.")
    toolchain = ["-DCMAKE_BUILD_TYPE=Release"]
    if properties.generator:
        toolchain += ["-G", properties.generator]
    if properties.platform:
        toolchain += ["-A", properties.platform]
    if properties.compiler:
        toolchain.append(f"-DCMAKE_C_COMPILER={properties.compiler}")
    if properties.flags:
        for kind in ("C", "EXE_LINKER", "SHARED_LINKER"):
            toolchain.append(f"-DCMAKE_{kind}_FLAGS={properties.flags}")
    toolchain += dependency_options(properties.triplet)
    library = []
    if properties.strict:
        library += ["-DCMAKE_C_STANDARD=11", "-DCMAKE_C_STANDARD_REQUIRED=ON"]
        library.append("-DCMAKE_C_FLAGS=-pedantic-errors")
    if properties.external_examples:
        library.append("-DX3PIO_BUILD_EXTERNAL_EXAMPLES=ON")

    for shared in ("ON", "OFF"):
        print(f"== BUILD_SHARED_LIBS={shared}")
        build, stage = directory / f"build-{shared}", directory / f"stage-{shared}"
        consumer = build / "consumer"
        run(
            "cmake -S .",
            *toolchain,
            *library,
            f"-DBUILD_SHARED_LIBS={shared}",
            "-DX3PIO_BUILD_EXAMPLES=ON",
            "-DCMAKE_COMPILE_WARNING_AS_ERROR=ON",
            f"-DCMAKE_INSTALL_PREFIX={stage}",
            "-B",
            build,
        )
        run("cmake --build", build, "--config", "Release")
        run("ctest --output-on-failure --no-tests=error -C Release --test-dir", build)
        run("cmake --install", build, "--config", "Release")
        # A static library needs the libraries that it was built with.
        prefix_path = ";".join(map(str, [stage, *vcpkg_installed(build)]))
        run(
            "cmake -S test_package",
            *toolchain,
            f"-DCMAKE_PREFIX_PATH={prefix_path}",
            "-B",
            consumer,
        )
        run("cmake --build", consumer, "--config", "Release")
        program = find(consumer, "test_package", "test_package.exe")
        run(shlex.quote(str(program)), env=library_env(stage / "lib", stage / "bin"))
        pc = find(stage, "x3pio.pc")
        if pc and shutil.which("pkg-config"):
            print(f"== pkg-config, BUILD_SHARED_LIBS={shared}")
            check_pkg_config(build, pc, static=shared == "OFF")


def configure(directory, options="", env=None):
    command = f"cmake -S . -G {NINJA} -DCMAKE_BUILD_TYPE=Debug {options}"
    run(command, *dependency_options(), "-B", directory, env=env)


def build_and_test(directory, env=None):
    run("cmake --build", directory)
    run("ctest --output-on-failure --no-tests=error --test-dir", directory, env=env)


def sanitizer(flags, directory):
    """ThreadSanitizer cannot be combined with AddressSanitizer, so each runs in
    a build of its own."""
    if os.environ.get("CI"):
        # Lowers the entropy of address-space layout randomization. At the
        # default setting of the runners, the sanitizers cannot map their
        # shadow memory.
        subprocess.run(["sudo", "sysctl", "vm.mmap_rnd_bits=28"])
    options = " ".join(
        f'"-DCMAKE_{kind}_FLAGS={flags} -fno-omit-frame-pointer"'
        for kind in ("C", "EXE_LINKER", "SHARED_LINKER")
    )
    configure(
        directory,
        f"-DCMAKE_COMPILE_WARNING_AS_ERROR=ON {options}",
        env=dict(os.environ, CC="clang"),
    )
    # The time functions of the C library, which minizip-ng calls for the time
    # stamps of the members, guard their state with locks that the sanitizer
    # does not see in a C library that it did not build.
    suppressions = ROOT / ".github" / "ci" / "tsan.supp"
    build_and_test(
        directory, env=dict(os.environ, TSAN_OPTIONS=f"suppressions={suppressions}")
    )


def sources(*patterns):
    paths = (path for pattern in patterns for path in ROOT.glob(pattern))
    return sorted(str(path.relative_to(ROOT)) for path in paths)


def static_analysis(directory):
    configure(directory, "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DX3PIO_BUILD_EXAMPLES=ON")
    files = sources("src/*.c", "tests/*.c", "examples/*.c", "test_package/*.c")
    # The files for Windows only need the headers of Windows.
    files = [name for name in files if "file_win32" not in name]
    run("clang-tidy --warnings-as-errors=* -p", directory, *files)


def check_format(directory):
    run(
        "clang-format --dry-run -Werror",
        *sources(
            "src/*.[ch]",
            "include/x3pio/*.h",
            "tests/*.[ch]",
            "test_package/*.c",
            "examples/*.c",
            "examples/external/*.cpp",
        ),
    )


def documentation(directory):
    options = "-DX3PIO_BUILD_DOCS=ON -DBUILD_TESTING=OFF"
    run(f"cmake -S . -G {NINJA} {options}", *dependency_options(), "-B", directory)
    run("cmake --build", directory, "--target", "docs")


def licenses(directory):
    run("reuse lint")


def coverage(directory):
    """Measures the coverage of the library sources by the tests, excluding the
    tests and the examples. The tests use threads, so the profiling counters are
    updated atomically. The job fails if fewer than 75 percent of the lines are
    executed, the threshold of related projects."""
    # The data of an earlier build, also for source files that do not exist any
    # more, would be read as well.
    shutil.rmtree(directory, ignore_errors=True)
    flags = "--coverage -fprofile-update=atomic"
    configure(directory, f'-DCMAKE_C_COMPILER=gcc "-DCMAKE_C_FLAGS={flags}"')
    build_and_test(directory)
    run(
        "gcovr --root . --filter src/ --exclude-throw-branches "
        "--fail-under-line 75 --print-summary --txt",
        directory / "coverage.txt",
        directory,
    )


CHECKS = {
    "asan-ubsan": partial(
        sanitizer, "-fsanitize=address,undefined -fno-sanitize-recover=undefined"
    ),
    "tsan": partial(sanitizer, "-fsanitize=thread"),
    "static-analysis": static_analysis,
    "format": check_format,
    "documentation": documentation,
    "licenses": licenses,
    "coverage": coverage,
}
JOBS = [*BUILDS, *CHECKS]


def main(arguments):
    if arguments == ["--list"]:
        print("\n".join(JOBS))
    elif len(arguments) == 1 and arguments[0] in JOBS:
        job = arguments[0]
        directory = ROOT / "build" / job
        if job in BUILDS:
            build_job(job, directory)
        else:
            CHECKS[job](directory)
    else:
        sys.exit("Usage: ci.py JOB | --list")


if __name__ == "__main__":
    main(sys.argv[1:])
