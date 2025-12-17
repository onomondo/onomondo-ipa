/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.32, section 5.14.8 Function (ESipa): CancelSession
 */

#include <stdint.h>
#include <errno.h>
#include <onomondo/ipa/http.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/ipad.h>
#include <EsipaMessageFromIpaToEim.h>
#include <CancelSessionRequestEsipa.h>
#include <CancelSessionResponseEsipa.h>
#include "utils.h"
#include "length.h"
#include "context.h"
#include "esipa.h"
#include "esipa_cancel_session.h"

static const struct num_str_map error_code_strings[] = {
	{ CancelSessionResponseEsipa__cancelSessionError_invalidTransactionId, "invalidTransactionId" },
	{ CancelSessionResponseEsipa__cancelSessionError_euiccSignatureInvalid, "euiccSignatureInvalid" },
	{ CancelSessionResponseEsipa__cancelSessionError_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static struct ipa_buf *enc_cancel_session_req(const struct ipa_esipa_cancel_session_req *req)
{
	struct EsipaMessageFromIpaToEim msg_to_eim = { 0 };

	msg_to_eim.present = EsipaMessageFromIpaToEim_PR_cancelSessionRequestEsipa;
	msg_to_eim.choice.cancelSessionRequestEsipa.transactionId = *req->transaction_id;

	if (req->cancel_session_ok) {
		msg_to_eim.choice.cancelSessionRequestEsipa.cancelSessionResponse.present =
		    SGP32_CancelSessionResponse_PR_cancelSessionResponseOk;
		msg_to_eim.choice.cancelSessionRequestEsipa.cancelSessionResponse.choice.cancelSessionResponseOk =
		    *req->cancel_session_ok;
	} else {
		msg_to_eim.choice.cancelSessionRequestEsipa.cancelSessionResponse.present =
		    SGP32_CancelSessionResponse_PR_cancelSessionResponseError;
		msg_to_eim.choice.cancelSessionRequestEsipa.cancelSessionResponse.choice.cancelSessionResponseError =
		    req->cancel_session_err;
	}

	/* Encode */
	return ipa_esipa_msg_to_eim_enc(&msg_to_eim, "CancelSession");
}

static struct ipa_esipa_cancel_session_res *dec_cancel_session_res(const struct ipa_buf *msg_to_ipa_encoded)
{
	struct EsipaMessageFromEimToIpa *msg_to_ipa = NULL;
	struct ipa_esipa_cancel_session_res *res = NULL;

	msg_to_ipa = ipa_esipa_msg_to_ipa_dec(msg_to_ipa_encoded, "CancelSession",
					      EsipaMessageFromEimToIpa_PR_cancelSessionResponseEsipa);
	if (!msg_to_ipa)
		return NULL;

	res = IPA_ALLOC_ZERO(struct ipa_esipa_cancel_session_res);
	res->msg_to_ipa = msg_to_ipa;

	switch (msg_to_ipa->choice.cancelSessionResponseEsipa.present) {
	case CancelSessionResponseEsipa_PR_cancelSessionOk:
		/* This function has no output data. The eIM is indicating a successful outcome through the presence
		 * of the CancelSessionOk field */
		res->cancel_session_ok = true;
		break;
	case CancelSessionResponseEsipa_PR_cancelSessionError:
		res->cancel_session_err = msg_to_ipa->choice.cancelSessionResponseEsipa.choice.cancelSessionError;
		IPA_LOGP_ESIPA("CancelSession", LERROR, "function failed with error code %ld=%s!\n",
			       res->cancel_session_err, ipa_str_from_num(error_code_strings, res->cancel_session_err,
									 "(unknown)"));
		break;
	default:
		IPA_LOGP_ESIPA("CancelSession", LERROR, "unexpected response content!\n");
		res->cancel_session_err = -1;
	}

	return res;
}

/*! Function (ESipa): CancelSession.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_esipa_cancel_session_res *ipa_esipa_cancel_session(struct ipa_context *ctx,
							      const struct ipa_esipa_cancel_session_req *req)
{
	struct ipa_buf *esipa_req = NULL;
	struct ipa_buf *esipa_res = NULL;
	struct ipa_esipa_cancel_session_res *res = NULL;

	IPA_LOGP_ESIPA("CancelSession", LINFO, "Requesting cancellation of session\n");

	esipa_req = enc_cancel_session_req(req);
	if (!esipa_req)
		goto error;

	esipa_res = ipa_esipa_req(ctx, esipa_req, "CancelSession");
	if (!esipa_res)
		goto error;

	res = dec_cancel_session_res(esipa_res);
	if (!res)
		goto error;

error:
	IPA_FREE(esipa_req);
	IPA_FREE(esipa_res);
	return res;
}

/*! Free results of function (ESipa): CancelSession.
 *  \param[in] res pointer to function result. */
void ipa_esipa_cancel_session_res_free(struct ipa_esipa_cancel_session_res *res)
{
	IPA_ESIPA_RES_FREE(res);
}
