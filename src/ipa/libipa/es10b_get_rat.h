/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <GetRatResponse.h>
struct ipa_context;

struct ipa_es10b_get_rat_res {
	struct GetRatResponse *res;
};

struct ipa_es10b_get_rat_res *ipa_es10b_get_rat(struct ipa_context *ctx);
void ipa_es10b_get_rat_res_free(struct ipa_es10b_get_rat_res *res);
