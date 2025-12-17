/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <PrepareDownloadRequest.h>
#include <PrepareDownloadResponse.h>
#include <PrepareDownloadResponseOk.h>
struct ipa_context;

/* GSMA SGP.22, section 5.7.5 */
struct ipa_es10b_prep_dwnld_req {
	struct PrepareDownloadRequest req;
};

struct ipa_es10b_prep_dwnld_res {
	struct PrepareDownloadResponse *res;
	struct PrepareDownloadResponseOk *prep_dwnld_ok;
	long prep_dwnld_err;
};

struct ipa_es10b_prep_dwnld_res *ipa_es10b_prep_dwnld(struct ipa_context *ctx,
						      const struct ipa_es10b_prep_dwnld_req *req);
void ipa_es10b_prep_dwnld_res_free(struct ipa_es10b_prep_dwnld_res *res);
