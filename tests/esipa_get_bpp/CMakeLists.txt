# Copyright (c) 2026 Onomondo ApS. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only

# mem.c gives the weak libc-forwarding allocator defaults, as in tests/mem.
add_executable(esipa_get_bpp_test esipa_get_bpp_test.c
	       ${PROJECT_SOURCE_DIR}/src/ipa/libipa/mem.c)
set_property(TARGET esipa_get_bpp_test PROPERTY C_STANDARD 99)
target_compile_options(esipa_get_bpp_test PRIVATE -Wall)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)

target_link_libraries(esipa_get_bpp_test libipa)
if (M32)
  set_target_properties(esipa_get_bpp_test PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
endif()

add_test(NAME esipa_get_bpp_test
    COMMAND sh -c "$<TARGET_FILE:esipa_get_bpp_test>")
