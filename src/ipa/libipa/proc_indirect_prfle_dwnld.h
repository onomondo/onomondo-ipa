/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdint.h>

struct ipa_context;
struct ipa_buf;

struct ipa_proc_indirect_prfle_dwnlod_pars {
	const char *ac;
	const uint8_t *tac;
	const struct ipa_buf *allowed_ca;
};

int ipa_proc_indirect_prfle_dwnlod(struct ipa_context *ctx, const struct ipa_proc_indirect_prfle_dwnlod_pars *pars);
