/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <AddInitialEimRequest.h>
#include <AddInitialEimResponse.h>
struct ipa_context;

/* GSMA SGP.32, section 5.9.4 */
struct ipa_es10b_add_init_eim_req {
	struct AddInitialEimRequest req;
};

struct ipa_es10b_add_init_eim_res {
	struct AddInitialEimResponse *res;
	long add_init_eim_err;
};

struct ipa_es10b_add_init_eim_res *ipa_es10b_add_init_eim(struct ipa_context *ctx,
							  const struct ipa_es10b_add_init_eim_req *req);
void ipa_es10b_add_init_eim_res_free(struct ipa_es10b_add_init_eim_res *res);
