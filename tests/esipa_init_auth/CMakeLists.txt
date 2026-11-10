# Copyright (c) 2026 Onomondo ApS. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only

# mem.c gives the weak libc-forwarding allocator defaults, as in tests/mem.
add_executable(esipa_init_auth_test esipa_init_auth_test.c
	       ${PROJECT_SOURCE_DIR}/src/ipa/libipa/mem.c)
set_property(TARGET esipa_init_auth_test PROPERTY C_STANDARD 99)
target_compile_options(esipa_init_auth_test PRIVATE -Wall)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/src/ipa/libipa)

target_link_libraries(esipa_init_auth_test libipa)
if (M32)
  set_target_properties(esipa_init_auth_test PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
endif()

add_test(NAME esipa_init_auth_test
    COMMAND sh -c "$<TARGET_FILE:esipa_init_auth_test>")
