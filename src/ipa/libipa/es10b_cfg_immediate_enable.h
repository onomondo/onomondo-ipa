/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdbool.h>

struct ipa_context;
struct ipa_buf;

struct ipa_es10b_cfg_immediate_enable_req {
	/*! request the eUICC to activate (true) or deactivate (false) immediate profile enabling */
	bool immediate_enable_flag;

	/*! OID of the default SM-DP+ (content octets), NULL = absent */
	const struct ipa_buf *smdp_oid;

	/*! address of the default SM-DP+, NULL = absent */
	const char *smdp_address;
};

int ipa_es10b_cfg_immediate_enable(struct ipa_context *ctx, const struct ipa_es10b_cfg_immediate_enable_req *req);
