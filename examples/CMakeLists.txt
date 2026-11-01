# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

foreach(example write_heights read_heights read_stream write_data read_data
        points layers coordinates vendor_files write_memory)
    add_executable(${example} ${example}.c)
    target_link_libraries(${example} PRIVATE x3pio)
    target_compile_options(${example} PRIVATE ${X3PIO_WARNINGS})
    target_compile_definitions(${example} PRIVATE
        $<$<PLATFORM_ID:Windows>:_CRT_SECURE_NO_WARNINGS>
    )
endforeach()
