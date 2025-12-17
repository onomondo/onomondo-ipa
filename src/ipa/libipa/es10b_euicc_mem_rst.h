/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdbool.h>
struct ipa_context;

/* GSMA SGP.22, section 5.7.13 */
struct ipa_es10b_euicc_mem_rst {
	bool operatnl_profiles;
	bool test_profiles;
	bool default_smdp_addr;
	bool eim_cfg_data;
	bool auto_enable_cfg;
};

int ipa_es10b_euicc_mem_rst(struct ipa_context *ctx, const struct ipa_es10b_euicc_mem_rst *req);
