# Copyright (c) 2026 Onomondo ApS. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only

# The tracking port needs glibc's malloc_usable_size().
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  include_directories(${PROJECT_SOURCE_DIR}/include)

  # Always built against the port (-U), whichever heap the build selects.
  add_executable(mem_debug_test mem_debug_test.c
		 ${PROJECT_SOURCE_DIR}/src/ipa/mem.c)
  set_property(TARGET mem_debug_test PROPERTY C_STANDARD 99)
  target_compile_options(mem_debug_test PRIVATE -Wall -UIPA_USE_SYSTEM_HEAP)
  if(M32)
    set_target_properties(mem_debug_test PROPERTIES COMPILE_FLAGS "-m32" LINK_FLAGS "-m32")
  endif()
  add_test(NAME mem_debug_test COMMAND sh -c "$<TARGET_FILE:mem_debug_test>")
endif()
