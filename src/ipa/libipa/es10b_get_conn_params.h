/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

struct ipa_context;
struct ipa_buf;

int ipa_es10b_get_conn_params(struct ipa_context *ctx, struct ipa_buf **http_params);
