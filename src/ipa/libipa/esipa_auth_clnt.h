/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <AuthenticateClientRequestEsipa.h>
#include <EsipaMessageFromEimToIpa.h>
#include <OCTET_STRING.h>
#include <AuthenticateClientOkDPEsipa.h>
#include <AuthenticateClientOkDSEsipa.h>
struct ipa_context;

struct ipa_esipa_auth_clnt_req {
	struct AuthenticateClientRequestEsipa req;
};

struct ipa_esipa_auth_clnt_res {
	struct EsipaMessageFromEimToIpa *msg_to_ipa;
	struct OCTET_STRING *transaction_id;
	struct AuthenticateClientOkDPEsipa *auth_clnt_ok_dpe;
	struct AuthenticateClientOkDSEsipa *auth_clnt_ok_dse;
	long auth_clnt_err;
};

struct ipa_esipa_auth_clnt_res *ipa_esipa_auth_clnt(struct ipa_context *ctx, const struct ipa_esipa_auth_clnt_req *req);
void ipa_esipa_auth_clnt_res_free(struct ipa_esipa_auth_clnt_res *res);
