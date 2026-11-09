# Copyright (c) 2026 Onomondo ApS. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only

# mem.c gives the weak libc-forwarding allocator defaults, as in tests/mem.
add_executable(euicc_transceive_test euicc_transceive_test.c
	       ${PROJECT_SOURCE_DIR}/src/ipa/libipa/mem.c)
set_property(TARGET euicc_transceive_test PROPERTY C_STANDARD 99)
target_compile_options(euicc_transceive_test PRIVATE -Wall)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/include)

target_link_libraries(euicc_transceive_test libipa)
if (M32)
  set_target_properties(euicc_transceive_test PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
endif()

add_test(NAME euicc_transceive_test
    COMMAND sh -c "$<TARGET_FILE:euicc_transceive_test>")
