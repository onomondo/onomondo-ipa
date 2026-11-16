# Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
# SPDX-License-Identifier: AGPL-3.0-only

enable_testing()

add_subdirectory(activation_code)
add_subdirectory(utils)
add_subdirectory(bpp_segments)
add_subdirectory(esipa_v12)
add_subdirectory(es10b_v12)
add_subdirectory(es10b_mem_rst)
add_subdirectory(mem)
add_subdirectory(euicc_transceive)
add_subdirectory(esipa_get_bpp)
add_subdirectory(esipa_init_auth)
add_subdirectory(epr_verbatim)
add_subdirectory(epr_hold)
