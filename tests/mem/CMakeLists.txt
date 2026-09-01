# Copyright (c) 2026 Onomondo ApS. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only

include_directories(${PROJECT_SOURCE_DIR}/include)

# mem_override_test links mem.c directly so both the weak defaults and the
# test's strong definitions are in the link, as in a platform's whole-archive
# build.
add_executable(mem_default_test mem_default_test.c)
add_executable(mem_override_test mem_override_test.c
	       ${PROJECT_SOURCE_DIR}/src/ipa/libipa/mem.c)
target_link_libraries(mem_default_test libipa)

foreach(t mem_default_test mem_override_test)
  set_property(TARGET ${t} PROPERTY C_STANDARD 99)
  target_compile_options(${t} PRIVATE -Wall)
  if(M32)
    set_target_properties(${t} PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
  endif()
  add_test(NAME ${t} COMMAND sh -c "$<TARGET_FILE:${t}>")
endforeach()
