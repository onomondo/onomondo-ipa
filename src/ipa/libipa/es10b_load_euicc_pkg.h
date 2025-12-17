/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <EuiccPackageRequest.h>
#include <EuiccPackageResult.h>
struct ipa_context;

struct ipa_es10b_load_euicc_pkg_req {
	struct EuiccPackageRequest req;
};

struct ipa_es10b_load_euicc_pkg_res {
	struct EuiccPackageResult *res;

	/*! set to true in case the EuiccPackageRequest contains an enable PSMO that executed successfully, which
	 *  means that a profile change actually took place. */
	bool profile_changed;

	/*! set to true in case the EuiccPackageRequest contains an enable PSMO that has the rollbackFlag set. */
	bool rollback_allowed;
};

struct ipa_es10b_load_euicc_pkg_res *ipa_es10b_load_euicc_pkg(struct ipa_context *ctx,
							      const struct ipa_es10b_load_euicc_pkg_req *req);
void ipa_es10b_load_euicc_pkg_res_free(struct ipa_es10b_load_euicc_pkg_res *res);
