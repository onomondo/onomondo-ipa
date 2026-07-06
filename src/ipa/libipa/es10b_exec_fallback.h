/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <Iccid.h>

struct ipa_context;
struct ipa_es10c_get_prfle_info_res;

int ipa_es10b_exec_fallback(struct ipa_context *ctx);

/* Helpers for the emulated fallback attribute, shared with the ReturnFromFallback driver and the
 * setFallbackAttribute/unsetFallbackAttribute PSMO emulation (es10b_load_euicc_pkg.c). */
bool iot_emu_iccid_eq(const Iccid_t *iccid, const uint8_t *raw, uint8_t raw_len);
bool iot_emu_fallback_prfle_exists(struct ipa_context *ctx, const struct ipa_es10c_get_prfle_info_res *prfle_info);
