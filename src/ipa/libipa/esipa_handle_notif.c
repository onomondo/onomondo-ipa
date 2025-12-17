/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.32, section 5.14.7: Function (ESipa): HandleNotification
 */

#include <stdint.h>
#include <errno.h>
#include <onomondo/ipa/http.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/ipad.h>
#include <EsipaMessageFromIpaToEim.h>
#include <EsipaMessageFromEimToIpa.h>
#include "utils.h"
#include "length.h"
#include "context.h"
#include "esipa.h"
#include "esipa_handle_notif.h"

static struct ipa_buf *enc_handle_notif_req(const struct ipa_esipa_handle_notif_req *req)
{
	struct EsipaMessageFromIpaToEim msg_to_eim = { 0 };
	struct SGP32_ProfileInstallationResult *prfle_inst_res;

	msg_to_eim.present = EsipaMessageFromIpaToEim_PR_handleNotificationEsipa;
	msg_to_eim.choice.handleNotificationEsipa.present = HandleNotificationEsipa_PR_pendingNotification;

	if (req->profile_installation_result) {
		msg_to_eim.choice.handleNotificationEsipa.choice.pendingNotification.present =
		    SGP32_PendingNotification_PR_profileInstallationResult;
		prfle_inst_res =
		    &msg_to_eim.choice.handleNotificationEsipa.choice.pendingNotification.choice.
		    profileInstallationResult;
		prfle_inst_res->profileInstallationResultData =
		    req->profile_installation_result->profileInstallationResultData;
		prfle_inst_res->euiccSignPIR = req->profile_installation_result->euiccSignPIR;
	} else if (req->pending_notification) {
		msg_to_eim.choice.handleNotificationEsipa.choice.pendingNotification = *req->pending_notification;
	} else {
		IPA_LOGP_ESIPA("HandleNotification", LERROR, "no notification data, nothing to deliver!\n");
		return NULL;
	}

	/* Encode */
	return ipa_esipa_msg_to_eim_enc(&msg_to_eim, "HandleNotification");
}

static int dec_handle_notif_res(const struct ipa_buf *msg_to_ipa_encoded)
{
	/* There is no response defined for this function (see also SGP.32, section 6.3.2.4), so we expect just an empty
	 * response (0 bytes of data) */
	if (msg_to_ipa_encoded->len) {
		IPA_LOGP_ESIPA("HandleNotification", LERROR,
			       "Expected a response of 0 bytes, but got a response with %zu bytes!\n",
			       msg_to_ipa_encoded->len);
		return -EINVAL;
	}

	return 0;
}

/*! Function (ESipa): HandleNotification.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns 0 on success, negative on error. */
int ipa_esipa_handle_notif(struct ipa_context *ctx, const struct ipa_esipa_handle_notif_req *req)
{
	struct ipa_buf *esipa_req = NULL;
	struct ipa_buf *esipa_res = NULL;
	int rc = -EINVAL;

	IPA_LOGP_ESIPA("HandleNotification", LINFO, "Sending notification to eIM\n");

	esipa_req = enc_handle_notif_req(req);
	if (!esipa_req)
		goto error;

	esipa_res = ipa_esipa_req(ctx, esipa_req, "HandleNotification");
	if (!esipa_res)
		goto error;

	rc = dec_handle_notif_res(esipa_res);
	if (rc < 0)
		goto error;

	rc = 0;
error:
	IPA_FREE(esipa_req);
	IPA_FREE(esipa_res);
	return rc;
}
