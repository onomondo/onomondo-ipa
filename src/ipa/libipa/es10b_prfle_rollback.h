/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <ProfileRollbackRequest.h>
#include <ProfileRollbackResponse.h>
struct ipa_context;

struct ipa_es10b_prfle_rollback_res {
	struct ProfileRollbackResponse *res;

	/*! the eUICCPackageResult of the response as the eUICC returned it (NULL if absent or emulated). */
	struct ipa_buf *epr_raw;
};

struct ipa_es10b_prfle_rollback_res *ipa_es10b_prfle_rollback(struct ipa_context *ctx, bool refresh_flag);
void ipa_es10b_prfle_rollback_res_free(struct ipa_es10b_prfle_rollback_res *res);
