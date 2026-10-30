# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

# Runs a program with the file as its standard input, as a pipe does.
execute_process(COMMAND "${PROGRAM}" INPUT_FILE "${INPUT}" RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "${PROGRAM} failed: ${result}")
endif()
