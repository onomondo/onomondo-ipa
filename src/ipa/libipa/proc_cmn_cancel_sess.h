/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <OCTET_STRING.h>
#include <CancelSessionReason.h>
struct ipa_context;

struct ipa_proc_cmn_cancel_sess_pars {
	long reason;
	struct OCTET_STRING transaction_id;
};

int ipa_proc_cmn_cancel_sess(struct ipa_context *ctx, const struct ipa_proc_cmn_cancel_sess_pars *pars);
