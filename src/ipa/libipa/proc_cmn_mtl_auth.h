/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdint.h>
struct ipa_buf;
struct ipa_esipa_auth_clnt_res;
struct ipa_context;

struct ipa_proc_cmn_mtl_auth_pars {
	const uint8_t *tac;
	const struct ipa_buf *allowed_ca;
	const char *smdp_addr;
	const char *ac_token;
};

struct ipa_esipa_auth_clnt_res *ipa_proc_cmn_mtl_auth(struct ipa_context *ctx,
						      const struct ipa_proc_cmn_mtl_auth_pars *pars);
