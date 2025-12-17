/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

struct ipa_buf;

struct ipa_buf *ipa_euicc_transceive_es10x(struct ipa_context *ctx, const struct ipa_buf *es10x_req);
int ipa_euicc_init_es10x(struct ipa_context *ctx);
int ipa_euicc_close_es10x(struct ipa_context *ctx);
