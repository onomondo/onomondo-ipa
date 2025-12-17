/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <EuiccPackageResult.h>
#include <RetrieveNotificationsListResponse.h>
#include <IpaEuiccDataResponse.h>
#include <ProfileDownloadTriggerResult.h>
#include <EsipaMessageFromEimToIpa.h>
#include <EimAcknowledgements.h>
struct ipa_context;

struct ipa_esipa_prvde_eim_pkg_rslt_req {
	long eim_pkg_err;
	const struct EuiccPackageResult *euicc_package_result;
	struct SGP32_RetrieveNotificationsListResponse *sgp32_notification_list;
	const struct IpaEuiccDataResponse *ipa_euicc_data_resp;
	const struct ProfileDownloadTriggerResult *prfle_dwnld_trig_req_rslt;
};

struct ipa_esipa_prvde_eim_pkg_rslt_res {
	struct EsipaMessageFromEimToIpa *msg_to_ipa;
	struct EimAcknowledgements *eim_acknowledgements;
};

struct ipa_esipa_prvde_eim_pkg_rslt_res *ipa_esipa_prvde_eim_pkg_rslt(struct ipa_context *ctx, const struct ipa_esipa_prvde_eim_pkg_rslt_req
								      *req);
void ipa_esipa_prvde_eim_pkg_rslt_free(struct ipa_esipa_prvde_eim_pkg_rslt_res *res);
