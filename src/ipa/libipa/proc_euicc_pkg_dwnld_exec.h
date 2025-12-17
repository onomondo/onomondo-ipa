/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <EuiccPackageRequest.h>
struct ipa_context;

struct ipa_proc_eucc_pkg_dwnld_exec_res {
	/*! flag to tell the caller that ipa_proc_eucc_pkg_dwnld_exec_onset must be called in order to complete the
	 *  procedure. */
	int call_onset;

	/*! cached EuiccPackageResult */
	struct ipa_es10b_load_euicc_pkg_res *load_euicc_pkg_res;

	struct ipa_es10b_prfle_rollback_res *prfle_rollback_res;

};

struct ipa_proc_eucc_pkg_dwnld_exec_res *ipa_proc_eucc_pkg_dwnld_exec(struct ipa_context *ctx, const struct EuiccPackageRequest
								      *euicc_package_request);
int ipa_proc_eucc_pkg_dwnld_exec_onset(struct ipa_context *ctx, struct ipa_proc_eucc_pkg_dwnld_exec_res *res);
void ipa_proc_eucc_pkg_dwnld_exec_res_free(struct ipa_proc_eucc_pkg_dwnld_exec_res *res);
