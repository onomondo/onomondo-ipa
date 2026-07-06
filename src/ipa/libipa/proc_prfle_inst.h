/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <BoundProfilePackage.h>
#include <ProfileInstallationResult.h>

struct ipa_context;

struct ipa_proc_prfle_inst_pars {
	const struct BoundProfilePackage *bound_profile_package;

	/* Out parameter (optional): when non-NULL, the ProfileInstallationResult the eUICC generated is
	 * stored here (ownership passes to the caller, free with ASN_STRUCT_FREE). The notification is
	 * still delivered to the eIM and removed from the eUICC by this procedure as usual. */
	struct ProfileInstallationResult **pir;
};

int ipa_proc_prfle_inst(struct ipa_context *ctx, const struct ipa_proc_prfle_inst_pars *pars);
