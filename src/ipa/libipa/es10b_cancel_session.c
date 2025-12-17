/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 * 
 * See also: GSMA SGP.22, 5.7.14: Function (ES10b): CancelSession
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include "context.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_cancel_session.h"

static const struct num_str_map error_code_strings[] = {
	{ CancelSessionResponse__cancelSessionResponseError_invalidTransactionId, "invalidTransactionId" },
	{ CancelSessionResponse__cancelSessionResponseError_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_cancel_session_res(struct ipa_es10b_cancel_session_res *res, const struct ipa_buf *es10b_res)
{
	struct CancelSessionResponse *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_CancelSessionResponse, es10b_res, "CancelSession");
	if (!asn)
		return -EINVAL;

	switch (asn->present) {
	case CancelSessionResponse_PR_cancelSessionResponseOk:
		res->cancel_session_ok = &asn->choice.cancelSessionResponseOk;
		break;
	case CancelSessionResponse_PR_cancelSessionResponseError:
		res->cancel_session_err = asn->choice.cancelSessionResponseError;
		IPA_LOGP_ES10X("CancelSession", LERROR, "function failed with error code %ld=%s!\n",
			       res->cancel_session_err, ipa_str_from_num(error_code_strings, res->cancel_session_err,
									 "(unknown)"));
		break;
	default:
		IPA_LOGP_ES10X("CancelSession", LERROR, "unexpected response content!\n");
		res->cancel_session_err = -1;
	}

	res->res = asn;
	return 0;
}

/*! Function (ES10b): CancelSession.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10b_cancel_session_res *ipa_es10b_cancel_session(struct ipa_context *ctx,
							      const struct ipa_es10b_cancel_session_req *req)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ipa_es10b_cancel_session_res *res = IPA_ALLOC_ZERO(struct ipa_es10b_cancel_session_res);
	int rc;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_CancelSessionRequest, &req->req, "CancelSession");
	if (!es10b_req) {
		IPA_LOGP_ES10X("CancelSession", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("CancelSession", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_cancel_session_res(res, es10b_res);
	if (rc < 0)
		goto error;

	if (res->cancel_session_ok
	    && !IPA_ASN_STR_CMP(&res->cancel_session_ok->euiccCancelSessionSigned.transactionId,
				&req->req.transactionId)) {
		IPA_LOGP_ES10X("CancelSession", LERROR,
			       "eUICC responded with unexpected transaction ID (expected: %s, got: %s)\n",
			       ipa_hexdump(req->req.transactionId.buf, req->req.transactionId.size),
			       ipa_hexdump(res->cancel_session_ok->euiccCancelSessionSigned.transactionId.buf,
					   res->cancel_session_ok->euiccCancelSessionSigned.transactionId.size));
		res->cancel_session_err = -1;
		goto error;
	}

	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return res;
error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	ipa_es10b_cancel_session_res_free(res);
	return NULL;
}

/*! Free results of function (ES10b): CancelSession.
 *  \param[in] res pointer to function result. */
void ipa_es10b_cancel_session_res_free(struct ipa_es10b_cancel_session_res *res)
{
	IPA_ES10X_RES_FREE(asn_DEF_CancelSessionResponse, res);
}
