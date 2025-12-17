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

static struct ipa_buf *enc_prvde_eim_pkg_rslt_req(const struct ipa_esipa_prvde_eim_pkg_rslt_req *req)
{
	struct EsipaMessageFromIpaToEim msg_to_eim = { 0 };
	struct ipa_buf *enc;

	msg_to_eim.present = EsipaMessageFromIpaToEim_PR_provideEimPackageResult;

	if (req->eim_pkg_err != 0) {
		msg_to_eim.choice.provideEimPackageResult.present = ProvideEimPackageResult_PR_eimPackageError;
		msg_to_eim.choice.provideEimPackageResult.choice.eimPackageError = req->eim_pkg_err;
	} else if (req->euicc_package_result && req->sgp32_notification_list) {
		msg_to_eim.choice.provideEimPackageResult.present = ProvideEimPackageResult_PR_ePRAndNotifications;
		msg_to_eim.choice.provideEimPackageResult.choice.ePRAndNotifications.euiccPackageResult =
		    *req->euicc_package_result;
		msg_to_eim.choice.provideEimPackageResult.choice.ePRAndNotifications.notificationList =
		    *req->sgp32_notification_list;
	} else if (req->euicc_package_result) {
		msg_to_eim.choice.provideEimPackageResult.present = ProvideEimPackageResult_PR_euiccPackageResult;
		msg_to_eim.choice.provideEimPackageResult.choice.euiccPackageResult = *req->euicc_package_result;
	} else if (req->ipa_euicc_data_resp) {
		msg_to_eim.choice.provideEimPackageResult.present = ProvideEimPackageResult_PR_ipaEuiccDataResponse;
		msg_to_eim.choice.provideEimPackageResult.choice.ipaEuiccDataResponse = *req->ipa_euicc_data_resp;
	} else if (req->prfle_dwnld_trig_req_rslt) {
		msg_to_eim.choice.provideEimPackageResult.present =
		    ProvideEimPackageResult_PR_profileDownloadTriggerResult;
		msg_to_eim.choice.provideEimPackageResult.choice.profileDownloadTriggerResult =
		    *req->prfle_dwnld_trig_req_rslt;
	} else {
		/* The struct should at least contain one of the above information element. In case the caller fails
		 * to fill out any of those, we fall back to setting eimPackageError to undefined. This will at least
		 * tell the eIM that something is wrong. */
		IPA_LOGP_ESIPA("ProvideEimPackageResult", LERROR,
			       "empty provideEimPackageResult request, setting eimPackageError to undefined\n");
		msg_to_eim.choice.provideEimPackageResult.present = ProvideEimPackageResult_PR_eimPackageError;
		msg_to_eim.choice.provideEimPackageResult.choice.eimPackageError =
		    ProvideEimPackageResult__eimPackageError_undefinedError;
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

	/* Optional, may be NULL */
	res->eim_acknowledgements = msg_to_ipa->choice.provideEimPackageResultResponse.eimAcknowledgements;

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

	esipa_req = enc_prvde_eim_pkg_rslt_req(req);
	if (!esipa_req)
		goto error;

	esipa_res = ipa_esipa_req(ctx, esipa_req, "ProvideEimPackageResult");
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
