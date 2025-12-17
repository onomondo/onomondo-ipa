/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdbool.h>
#include <EUICCInfo1.h>
#include <EUICCInfo2.h>
#include <SGP32-EUICCInfo2.h>

/* GSMA SGP.22, section 5.7.8 */
struct ipa_es10b_euicc_info {
	struct EUICCInfo1 *euicc_info_1;
	struct EUICCInfo2 *euicc_info_2;
	struct SGP32_EUICCInfo2 *sgp32_euicc_info_2;

	/*! When the IoT eUICC emulation is enabled, this function will retrieve the euicc_info_2 (SGP.22) from the
	 *  eUICC and derive sgp32_euicc_info_2 from it. When the emulation is turned off sgp32_euicc_info_2 will
	 *  be retrieved directly and no conversion is needed. This also means that euicc_info_2 (SGP.22) will be left
	 *  unpopulated (NULL) in this case. Hence it is recommended to use sgp32_euicc_info_2 only, since it will
	 *  always be populated, regardless of which eUICC type is used. */
};

struct ipa_es10b_euicc_info *ipa_es10b_get_euicc_info(struct ipa_context *ctx, bool full);
void ipa_es10b_get_euicc_info_free(struct ipa_es10b_euicc_info *res);
