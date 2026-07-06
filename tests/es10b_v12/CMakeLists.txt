# Copyright (c) 2026 Onomondo ApS. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only

add_executable(es10b_v12_test es10b_v12_test.c)
set_property(TARGET es10b_v12_test PROPERTY C_STANDARD 99)
target_compile_options(es10b_v12_test PRIVATE -Wall)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)

target_link_libraries(es10b_v12_test libipa)
if (M32)
  set_target_properties(es10b_v12_test PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
endif()

add_test(NAME es10b_v12_test
    COMMAND sh -c "$<TARGET_FILE:es10b_v12_test>")
