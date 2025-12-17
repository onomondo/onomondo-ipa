/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <CancelSessionRequest.h>
#include <CancelSessionResponse.h>
#include <CancelSessionResponseOk.h>
struct ipa_context;

/* GSMA SGP.22, section 5.7.14 */
struct ipa_es10b_cancel_session_req {
	struct CancelSessionRequest req;
};

struct ipa_es10b_cancel_session_res {
	struct CancelSessionResponse *res;
	struct CancelSessionResponseOk *cancel_session_ok;
	long cancel_session_err;
};

struct ipa_es10b_cancel_session_res *ipa_es10b_cancel_session(struct ipa_context *ctx,
							      const struct ipa_es10b_cancel_session_req *req);
void ipa_es10b_cancel_session_res_free(struct ipa_es10b_cancel_session_res *res);
