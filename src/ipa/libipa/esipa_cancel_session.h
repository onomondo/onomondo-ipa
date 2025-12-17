/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdbool.h>
#include <OCTET_STRING.h>
#include <CancelSessionResponseOk.h>
#include <EsipaMessageFromEimToIpa.h>
struct ipa_context;

struct ipa_esipa_cancel_session_req {
	struct OCTET_STRING *transaction_id;
	struct CancelSessionResponseOk *cancel_session_ok;
	long cancel_session_err;
};

struct ipa_esipa_cancel_session_res {
	struct EsipaMessageFromEimToIpa *msg_to_ipa;
	bool cancel_session_ok;
	long cancel_session_err;
};

struct ipa_esipa_cancel_session_res *ipa_esipa_cancel_session(struct ipa_context *ctx,
							      const struct ipa_esipa_cancel_session_req *req);
void ipa_esipa_cancel_session_res_free(struct ipa_esipa_cancel_session_res *res);
