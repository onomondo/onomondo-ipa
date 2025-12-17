/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <AuthenticateServerRequest.h>
#include <AuthenticateServerResponse.h>
#include <AuthenticateResponseOk.h>
struct ipa_context;

/* GSMA SGP.22, section 5.7.13 */
struct ipa_es10b_auth_serv_req {
	struct AuthenticateServerRequest req;
};

struct ipa_es10b_auth_serv_res {
	struct AuthenticateServerResponse *res;
	struct AuthenticateResponseOk *auth_serv_ok;
	long auth_serv_err;
};

struct ipa_es10b_auth_serv_res *ipa_es10b_auth_serv(struct ipa_context *ctx, const struct ipa_es10b_auth_serv_req *req);
void ipa_es10b_auth_serv_res_free(struct ipa_es10b_auth_serv_res *res);
