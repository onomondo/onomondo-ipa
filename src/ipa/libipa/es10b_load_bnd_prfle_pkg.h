/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <ProfileInstallationResult.h>
struct ipa_context;

struct ipa_es10b_load_bnd_prfle_pkg_res {
	struct ProfileInstallationResult *res;
};

struct ipa_es10b_load_bnd_prfle_pkg_res *ipa_es10b_load_bnd_prfle_pkg(struct ipa_context *ctx, const uint8_t *segment,
								      size_t segment_len);
void ipa_es10b_load_bnd_prfle_res_free(struct ipa_es10b_load_bnd_prfle_pkg_res *res);
