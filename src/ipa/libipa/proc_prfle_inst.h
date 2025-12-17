/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <BoundProfilePackage.h>

struct ipa_context;

struct ipa_proc_prfle_inst_pars {
	const struct BoundProfilePackage *bound_profile_package;
};

int ipa_proc_prfle_inst(struct ipa_context *ctx, const struct ipa_proc_prfle_inst_pars *pars);
