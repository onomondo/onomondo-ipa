/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <DeleteProfileRequest.h>
#include <DeleteProfileResponse.h>
struct ipa_context;

struct ipa_es10c_delete_prfle_req {
	struct DeleteProfileRequest req;
};

struct ipa_es10c_delete_prfle_res {
	struct DeleteProfileResponse *res;
};

struct ipa_es10c_delete_prfle_res *ipa_es10c_delete_prfle(struct ipa_context *ctx,
							  const struct ipa_es10c_delete_prfle_req *req);
void ipa_es10c_delete_prfle_res_free(struct ipa_es10c_delete_prfle_res *res);
