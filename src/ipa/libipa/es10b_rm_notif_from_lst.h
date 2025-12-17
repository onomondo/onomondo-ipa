/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

struct ipa_context;

int ipa_es10b_rm_notif_from_lst(struct ipa_context *ctx, long seq_number);
