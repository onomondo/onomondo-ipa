/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdint.h>
struct ipa_context;

int ipa_es10b_get_euicc_chlg(struct ipa_context *ctx, uint8_t *euicc_chlg);
