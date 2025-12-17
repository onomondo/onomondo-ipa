/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <PrepareDownloadResponse.h>
#include <EsipaMessageFromEimToIpa.h>
#include <GetBoundProfilePackageOkEsipa.h>
struct ipa_context;

struct ipa_esipa_get_bnd_prfle_pkg_req {
	const struct PrepareDownloadResponse *prep_dwnld_res;
};

struct ipa_esipa_get_bnd_prfle_pkg_res {
	struct EsipaMessageFromEimToIpa *msg_to_ipa;
	struct GetBoundProfilePackageOkEsipa *get_bnd_prfle_pkg_ok;
	long get_bnd_prfle_pkg_err;
};

struct ipa_esipa_get_bnd_prfle_pkg_res *ipa_esipa_get_bnd_prfle_pkg(struct ipa_context *ctx,
								    const struct ipa_esipa_get_bnd_prfle_pkg_req *req);
void ipa_esipa_get_bnd_prfle_pkg_res_free(struct ipa_esipa_get_bnd_prfle_pkg_res *res);
