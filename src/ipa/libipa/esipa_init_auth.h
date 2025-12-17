/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <EUICCInfo1.h>
#include <EsipaMessageFromEimToIpa.h>
#include <InitiateAuthenticationOkEsipa.h>
struct ipa_context;

struct ipa_esipa_init_auth_req {
	const uint8_t *euicc_challenge;
	const char *smdp_addr;
	const EUICCInfo1_t *euicc_info_1;
};

struct ipa_esipa_init_auth_res {
	struct EsipaMessageFromEimToIpa *msg_to_ipa;
	struct InitiateAuthenticationOkEsipa *init_auth_ok;
	long init_auth_err;
};

struct ipa_esipa_init_auth_res *ipa_esipa_init_auth(struct ipa_context *ctx, const struct ipa_esipa_init_auth_req *req);
void ipa_esipa_init_auth_res_free(struct ipa_esipa_init_auth_res *res);
