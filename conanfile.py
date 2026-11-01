# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

"""Conan 2 recipe building libx3pio from this working tree.

Use it to consume the library from a local checkout::

    conan create .

or to develop against it, which brings the dependencies with it::

    conan install . --build=missing
    cmake --preset conan-release
"""

import os
import re

from conan import ConanFile
from conan.errors import ConanException
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load, rmdir
from conan.tools.gnu import PkgConfigDeps

required_conan_version = ">=2.0"


class X3pioConan(ConanFile):
    name = "x3pio"
    package_type = "library"
    license = "MIT"
    homepage = "https://github.com/aschet/libx3pio"
    url = homepage
    description = "Reader and writer for ISO 25178-72 x3p files in C"
    topics = ("x3p", "iso-25178", "surface-texture", "topography", "metrology")

    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        # A container of x3p uses deflate and nothing else: no encryption and
        # none of the other codecs, so no more than zlib is needed.
        "minizip-ng/*:with_bzip2": False,
        "minizip-ng/*:with_lzma": False,
        "minizip-ng/*:with_zstd": False,
        "minizip-ng/*:with_openssl": False,
    }
    implements = ["auto_shared_fpic"]

    exports_sources = (
        "CMakeLists.txt",
        "LICENSE",
        "cmake/*",
        "examples/*",
        "include/*",
        "src/*",
        "tests/*",
    )

    @property
    def _skip_test(self):
        return bool(self.conf.get("tools.build:skip_test", default=False))

    def set_version(self):
        # The project declares the version once, in CMakeLists.txt.
        text = load(self, os.path.join(self.recipe_folder, "CMakeLists.txt"))
        match = re.search(r"project\(.*?VERSION\s+(\d+\.\d+\.\d+)", text, re.S)
        if match is None:
            raise ConanException("no project version found in CMakeLists.txt")
        self.version = match.group(1)

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")
        if self.settings.os != "Windows":
            # Names of the zip members are UTF-8, there is nothing to convert.
            self.options["minizip-ng"].with_iconv = False
            self.options["minizip-ng"].with_libbsd = False

    def requirements(self):
        self.requires("minizip-ng/[>=4.0.3 <5]")
        self.requires("expat/[>=2.6 <3]")

    def build_requirements(self):
        # Packaging runs with tests skipped and never resolves these.
        if not self._skip_test:
            self.test_requires("libxml2/[>=2.12 <3]")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.cache_variables["BUILD_TESTING"] = not self._skip_test
        toolchain.generate()
        CMakeDeps(self).generate()
        # The pc files of the dependencies, for the check of the pc file of the
        # library with pkg-config. Conan Center names the one of minizip-ng
        # minizip, and the pc file of the library requires minizip-ng, as
        # upstream and vcpkg name it.
        pc = PkgConfigDeps(self)
        pc.set_property("minizip-ng::minizip", "pkg_config_aliases", ["minizip-ng"])
        pc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.test()  # a no-op when tools.build:skip_test is set

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        # Conan generates its own package files from the metadata below.
        for unused in ("lib/cmake", "lib/pkgconfig", "share/licenses"):
            rmdir(self, os.path.join(self.package_folder, unused))

    def package_info(self):
        postfix = (
            "d"
            if self.settings.os == "Windows"
            and self.settings.build_type == "Debug"
            else ""
        )
        self.cpp_info.libs = ["x3pio" + postfix]
        if not self.options.shared:
            self.cpp_info.defines = ["X3PIO_STATIC_DEFINE"]
        self.cpp_info.set_property("cmake_file_name", "x3pio")
        self.cpp_info.set_property("cmake_target_name", "x3pio::x3pio")
        self.cpp_info.set_property("pkg_config_name", "x3pio")
