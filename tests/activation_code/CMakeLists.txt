# Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only

add_executable(activation_code_test activation_code_test.c)
set_property(TARGET activation_code_test PROPERTY C_STANDARD 99)
target_compile_options(activation_code_test PRIVATE -Wall)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)

target_link_libraries(activation_code_test libipa)
if (M32)
  set_target_properties(activation_code_test PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
endif()

add_test(NAME activation_code_test
    COMMAND sh -c "$<TARGET_FILE:activation_code_test> > activation_code_test.out 2> activation_code_test.err")

add_test(NAME activation_code_compare_stdout
    COMMAND ${CMAKE_COMMAND} -E compare_files
    ${PROJECT_SOURCE_DIR}/build/tests/activation_code/activation_code_test.out 
    ${CMAKE_CURRENT_SOURCE_DIR}/activation_code_test.ok)

add_test(NAME activation_code_compare_stderr
    COMMAND ${CMAKE_COMMAND} -E compare_files
    ${PROJECT_SOURCE_DIR}/build/tests/activation_code/activation_code_test.err 
    ${CMAKE_CURRENT_SOURCE_DIR}/activation_code_test.err)
