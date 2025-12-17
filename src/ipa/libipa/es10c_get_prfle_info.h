/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <ProfileInfoListRequest.h>
#include <ProfileInfoListResponse.h>
#include <SGP32-ProfileInfoListResponse.h>
struct ipa_context;

struct ipa_es10c_get_prfle_info_req {
	struct ProfileInfoListRequest req;
};

struct ipa_es10c_get_prfle_info_res {
	struct ProfileInfoListResponse *res;
	struct SGP32_ProfileInfoListResponse *sgp32_res;
	struct ProfileInfo *currently_active_prfle;
	long prfle_info_list_err;

	/*! When the IoT eUICC emulation is enabled, this function will retrieve the ProfileInfoListResponse
	 *  (res, SGP.22) from the eUICC and derive SGP32_ProfileInfoListResponse (sgp32_res) from it. When
	 *  the emulation is turned off sgp32_res will be retrieved directly and no conversion is needed. This also
	 *  means that res (SGP.22) will be left unpopulated (NULL) in this case. Hence it is recommended to use
	 *  sgp32_res only, since it will always be populated, regardless of which eUICC type is used. */
};

struct ipa_es10c_get_prfle_info_res *ipa_es10c_get_prfle_info(struct ipa_context *ctx,
							      const struct ipa_es10c_get_prfle_info_req *req);
void ipa_es10c_get_prfle_info_res_free(struct ipa_es10c_get_prfle_info_res *res);
