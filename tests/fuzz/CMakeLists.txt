# Copyright (c) 2026 Onomondo ApS. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only

include_directories(${PROJECT_SOURCE_DIR}/include)

add_executable(fuzz_ber_decode fuzz_ber_decode.c)
set_property(TARGET fuzz_ber_decode PROPERTY C_STANDARD 99)
target_compile_options(fuzz_ber_decode PRIVATE -Wall)
# libasn first: the driver itself only calls libasn, whose allocator calls resolve
# to the ipa_port_* defaults in libipa, and GNU ld scans archives left to right.
target_link_libraries(fuzz_ber_decode libasn libipa)

if(ENABLE_FUZZ)
  # Real libFuzzer binary (clang): -fsanitize=fuzzer provides main().
  target_compile_options(fuzz_ber_decode PRIVATE -fsanitize=fuzzer,address -DIPA_FUZZ_ENGINE)
  target_link_options(fuzz_ber_decode PRIVATE -fsanitize=fuzzer,address)
else()
  # Replay driver: the corpus doubles as a decode smoke test.
  add_test(NAME fuzz_ber_decode_replay
      COMMAND sh -c "$<TARGET_FILE:fuzz_ber_decode> ${CMAKE_CURRENT_SOURCE_DIR}/corpus/*")
endif()
