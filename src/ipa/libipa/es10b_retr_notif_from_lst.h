/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <RetrieveNotificationsListRequest.h>
#include <RetrieveNotificationsListResponse.h>
#include <IpaEuiccDataRequest.h>
#include <SGP32-RetrieveNotificationsListRequest.h>
#include <SGP32-RetrieveNotificationsListResponse.h>
struct ipa_context;

struct ipa_es10b_retr_notif_from_lst_req {
	struct SGP32_RetrieveNotificationsListRequest__searchCriteria search_criteria;

	/* When the caller has access to a search searchCriteria that originates from an IpaEuiccDataRequest, the
	 * pointer to it may be passed here. The function will then atomatically convert the searchCriteria into
	 * the native RetrieveNotificationsListRequest format. */
	const struct IpaEuiccDataRequest__searchCriteria *dr_search_criteria;
};

struct ipa_es10b_retr_notif_from_lst_res {
	struct RetrieveNotificationsListResponse *res;
	struct SGP32_RetrieveNotificationsListResponse *sgp32_res;
	long notif_lst_result_err;

	/*! When the IoT eUICC emulation is enabled, this function will retrieve the RetrieveNotificationsListResponse
	 *  (res, SGP.22) from the eUICC and derive SGP32_RetrieveNotificationsListResponse (sgp32_res) from it. When
	 *  the emulation is turned off sgp32_res will be retrieved directly and no conversion is needed. This also
	 *  means that res (SGP.22) will be left unpopulated (NULL) in this case. Hence it is recommended to use
	 *  sgp32_res only, since it will always be populated, regardless of which eUICC type is used. */
};

struct ipa_es10b_retr_notif_from_lst_res *ipa_es10b_retr_notif_from_lst(struct ipa_context *ctx, const struct ipa_es10b_retr_notif_from_lst_req
									*req);
void ipa_es10b_retr_notif_from_lst_res_free(struct ipa_es10b_retr_notif_from_lst_res *res);
