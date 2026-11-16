# Copyright (c) 2026 Onomondo ApS. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only

# mem.c gives the weak libc-forwarding allocator defaults, as in tests/mem.
add_executable(epr_hold_test epr_hold_test.c
	       ${PROJECT_SOURCE_DIR}/src/ipa/libipa/mem.c)
set_property(TARGET epr_hold_test PROPERTY C_STANDARD 99)
target_compile_options(epr_hold_test PRIVATE -Wall)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/src/ipa/libipa)

target_link_libraries(epr_hold_test libipa)
if (M32)
  set_target_properties(epr_hold_test PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
endif()

add_test(NAME epr_hold_test
    COMMAND sh -c "$<TARGET_FILE:epr_hold_test>")
