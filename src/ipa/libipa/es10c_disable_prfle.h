/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <DisableProfileRequest.h>
#include <DisableProfileResponse.h>
struct ipa_context;

struct ipa_es10c_disable_prfle_req {
	struct DisableProfileRequest req;
};

struct ipa_es10c_disable_prfle_res {
	struct DisableProfileResponse *res;
};

struct ipa_es10c_disable_prfle_res *ipa_es10c_disable_prfle(struct ipa_context *ctx,
							    const struct ipa_es10c_disable_prfle_req *req);
void ipa_es10c_disable_prfle_res_free(struct ipa_es10c_disable_prfle_res *res);
