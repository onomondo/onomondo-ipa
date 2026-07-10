/*
 * Copyrighct (c) 2025 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 * 
 * See also: GSMA SGP.32, section 5.14.6: Function (ESipa): ProvideEimPackageResult
 */

#include <stdint.h>
#include <onomondo/ipa/http.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/ipad.h>
#include <EsipaMessageFromIpaToEim.h>
#include <ProvideEimPackageResult.h>
#include <SGP32-RetrieveNotificationsListResponse.h>
#include "utils.h"
#include "length.h"
#include "context.h"
#include "esipa.h"
#include "esipa_prvde_eim_pkg_rslt.h"

/* Pick the bare notification list ('A0') out of the RetrieveNotificationsListResponse the eUICC
 * returned. In SGP.32 v1.2 only the list itself is transferred to the eIM, no longer the whole
 * BF2B response. */
static void set_notification_list(struct PendingNotificationList *notification_list,
				  const struct SGP32_RetrieveNotificationsListResponse *rsp)
{
	switch (rsp->present) {
	case SGP32_RetrieveNotificationsListResponse_PR_notificationList:
		notification_list->list.array = rsp->choice.notificationList.list.array;
		notification_list->list.count = rsp->choice.notificationList.list.count;
		notification_list->list.size = rsp->choice.notificationList.list.size;
		break;
	case SGP32_RetrieveNotificationsListResponse_PR_notificationAndEprList:
		notification_list->list.array = rsp->choice.notificationAndEprList.notificationList.list.array;
		notification_list->list.count = rsp->choice.notificationAndEprList.notificationList.list.count;
		notification_list->list.size = rsp->choice.notificationAndEprList.notificationList.list.size;
		break;
	default:
		/* No notifications to report (e.g. the eUICC answered with an error), the
		 * notificationList stays empty. */
		break;
	}
}

static struct ipa_buf *enc_prvde_eim_pkg_rslt_req(struct ipa_context *ctx,
						  const struct ipa_esipa_prvde_eim_pkg_rslt_req *req)
{
	struct EsipaMessageFromIpaToEim msg_to_eim = { 0 };
	struct ProvideEimPackageResult *pkg_rslt = &msg_to_eim.choice.provideEimPackageResult;
	OCTET_STRING_t eid_value = { 0 };
	struct PendingNotificationList notification_list = { 0 };
	struct ipa_buf *enc;

	msg_to_eim.present = EsipaMessageFromIpaToEim_PR_provideEimPackageResult;

	/* Identify the eUICC towards the eIM (optional in v1.2, but there is no reason to omit it) */
	eid_value.buf = (uint8_t *)ctx->eid;
	eid_value.size = sizeof(ctx->eid);
	pkg_rslt->eidValue = &eid_value;

	if (req->eim_pkg_err != 0) {
		pkg_rslt->eimPackageResult.present = EimPackageResult_PR_eimPackageResultResponseError;
		pkg_rslt->eimPackageResult.choice.eimPackageResultResponseError.eimPackageResultErrorCode =
		    req->eim_pkg_err;
		pkg_rslt->eimPackageResult.choice.eimPackageResultResponseError.eimTransactionId =
		    (TransactionId_t *)req->eim_transaction_id;
	} else if (req->euicc_package_result && req->sgp32_notification_list) {
		pkg_rslt->eimPackageResult.present = EimPackageResult_PR_ePRAndNotifications;
		pkg_rslt->eimPackageResult.choice.ePRAndNotifications.euiccPackageResult =
		    *req->euicc_package_result;
		set_notification_list(&notification_list, req->sgp32_notification_list);
		pkg_rslt->eimPackageResult.choice.ePRAndNotifications.notificationList = notification_list;
	} else if (req->euicc_package_result) {
		pkg_rslt->eimPackageResult.present = EimPackageResult_PR_euiccPackageResult;
		pkg_rslt->eimPackageResult.choice.euiccPackageResult = *req->euicc_package_result;
	} else if (req->ipa_euicc_data_resp) {
		pkg_rslt->eimPackageResult.present = EimPackageResult_PR_ipaEuiccDataResponse;
		pkg_rslt->eimPackageResult.choice.ipaEuiccDataResponse = *req->ipa_euicc_data_resp;
	} else if (req->prfle_dwnld_trig_req_rslt) {
		pkg_rslt->eimPackageResult.present = EimPackageResult_PR_profileDownloadTriggerResult;
		pkg_rslt->eimPackageResult.choice.profileDownloadTriggerResult = *req->prfle_dwnld_trig_req_rslt;
	} else {
		/* The struct should at least contain one of the above information element. In case the caller fails
		 * to fill out any of those, we fall back to setting the error code to undefined. This will at least
		 * tell the eIM that something is wrong. */
		IPA_LOGP_ESIPA("ProvideEimPackageResult", LERROR,
			       "empty provideEimPackageResult request, setting eimPackageResultErrorCode to undefined\n");
		pkg_rslt->eimPackageResult.present = EimPackageResult_PR_eimPackageResultResponseError;
		pkg_rslt->eimPackageResult.choice.eimPackageResultResponseError.eimPackageResultErrorCode =
		    EimPackageResultErrorCode_undefinedError;
	}

	enc = ipa_esipa_msg_to_eim_enc(&msg_to_eim, "ProvideEimPackageResult");

	return enc;
}

struct ipa_esipa_prvde_eim_pkg_rslt_res *dec_prvde_eim_pkg_rslt_res(const struct ipa_buf *msg_to_ipa_encoded)
{
	struct EsipaMessageFromEimToIpa *msg_to_ipa = NULL;
	struct ipa_esipa_prvde_eim_pkg_rslt_res *res = NULL;

	msg_to_ipa =
	    ipa_esipa_msg_to_ipa_dec(msg_to_ipa_encoded, "ProvideEimPackageResult",
				     EsipaMessageFromEimToIpa_PR_provideEimPackageResultResponse);
	if (!msg_to_ipa)
		return NULL;

	res = IPA_ALLOC_ZERO(struct ipa_esipa_prvde_eim_pkg_rslt_res);
	res->msg_to_ipa = msg_to_ipa;

	switch (msg_to_ipa->choice.provideEimPackageResultResponse.present) {
	case ProvideEimPackageResultResponse_PR_eimAcknowledgements:
		res->eim_acknowledgements = &msg_to_ipa->choice.provideEimPackageResultResponse.choice.eimAcknowledgements;
		break;
	case ProvideEimPackageResultResponse_PR_emptyResponse:
		/* Nothing to acknowledge */
		break;
	case ProvideEimPackageResultResponse_PR_provideEimPackageResultError:
		IPA_LOGP_ESIPA("ProvideEimPackageResult", LERROR, "eIM reports error code %ld!\n",
			       msg_to_ipa->choice.provideEimPackageResultResponse.choice.provideEimPackageResultError);
		break;
	default:
		IPA_LOGP_ESIPA("ProvideEimPackageResult", LERROR, "unexpected response content!\n");
		break;
	}

	return res;
}

/*! Function (ESipa): ProvideEimPackageResult.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_esipa_prvde_eim_pkg_rslt_res *ipa_esipa_prvde_eim_pkg_rslt(struct ipa_context *ctx, const struct ipa_esipa_prvde_eim_pkg_rslt_req
								      *req)
{
	struct ipa_buf *esipa_req = NULL;
	struct ipa_buf *esipa_res = NULL;
	struct ipa_esipa_prvde_eim_pkg_rslt_res *res = NULL;

	IPA_LOGP_ESIPA("ProvideEimPackageResult", LINFO,
		       "Providing eUICC package result and eUICC notifications to eIM\n");

	esipa_req = enc_prvde_eim_pkg_rslt_req(ctx, req);
	if (!esipa_req)
		goto error;

	esipa_res = ipa_esipa_req(ctx, esipa_req, "ProvideEimPackageResult");
	IPA_FREE(esipa_req);
	esipa_req = NULL;
	if (!esipa_res)
		goto error;

	res = dec_prvde_eim_pkg_rslt_res(esipa_res);
	if (!res)
		goto error;

error:
	IPA_FREE(esipa_req);
	IPA_FREE(esipa_res);
	return res;
}

/*! Free results of function (ESipa): ProvideEimPackageResult.
 *  \param[in] res pointer to function result. */
void ipa_esipa_prvde_eim_pkg_rslt_free(struct ipa_esipa_prvde_eim_pkg_rslt_res *res)
{
	IPA_ESIPA_RES_FREE(res);
}
