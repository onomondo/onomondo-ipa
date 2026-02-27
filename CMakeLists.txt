# Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only

cmake_minimum_required(VERSION 3.20.0 FATAL_ERROR)

# Only take over as a top-level Zephyr project when built standalone (i.e. not
# included via add_subdirectory from an existing Zephyr build such as nrf-ipa).
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND DEFINED ENV{ZEPHYR_BASE})
  find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
endif()

project(ipa VERSION 0.0.1 LANGUAGES C)

option(ENABLE_SANITIZE "Compile with address sanitizer enabled" OFF)
option(SHOW_ASN_OUTPUT "Show ASN.1 decoded output in the log" OFF)
option(ASN_EMIT_DEBUG "Enable built-in debug output of asn1c" OFF)
option(MEM_EMIT_DEBUG "Enable debug output for heap memory usage" OFF)
option(M32 "Compile for 32 bit mode" OFF)

if(ENABLE_SANITIZE)
  add_compile_options(-fsanitize=address)
  add_link_options(-fsanitize=address)
endif()

if(SHOW_ASN_OUTPUT)
  add_definitions(-DSHOW_ASN_OUTPUT)
endif()

if(ASN_EMIT_DEBUG)
  add_definitions(-DASN_EMIT_DEBUG)
endif()

if(MEM_EMIT_DEBUG)
  add_definitions(-DMEM_EMIT_DEBUG)
endif()

if(TARGET zephyr_interface)
  # ── Zephyr / NCS build ──────────────────────────────────────────────────────
  # The platform-specific http.c and scard.c are provided by the
  # onomondo-ipa-compat library built from nrf-ipa/lib/onomondo_ipa_compat/.
  # That library is registered via zephyr_library_named() so it is linked
  # automatically – no explicit zephyr_link_libraries() call needed here.
  zephyr_library()
  add_subdirectory_ifdef(CONFIG_ONOMONDO_IPA src)
  zephyr_include_directories(include)
else()
  # ── Standalone Linux build ───────────────────────────────────────────────────
  add_subdirectory(src)
  include(CTest)
  add_subdirectory(tests)
endif()

