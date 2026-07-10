/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

struct ipa_context;
struct ipa_esipa_get_bnd_prfle_pkg_res;
struct ipa_esipa_auth_clnt_res;

struct ipa_proc_prfle_dwnlod_pars {
	/* In/out: consumed (freed and NULLed) as soon as the eUICC has processed PrepareDownload, so that the
	 * multi-kB AuthenticateClient response tree is gone before the even larger GetBoundProfilePackage
	 * response is received and decoded. The caller must not rely on any pointer into it afterwards. */
	struct ipa_esipa_auth_clnt_res **auth_clnt_res;
};

struct ipa_esipa_get_bnd_prfle_pkg_res *ipa_proc_prfle_dwnlod(struct ipa_context *ctx,
							      const struct ipa_proc_prfle_dwnlod_pars *pars);
