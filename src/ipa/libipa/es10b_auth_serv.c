/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, 5.7.13: Function (ES10b): AuthenticateServer
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
#include "es10b_auth_serv.h"

static const struct num_str_map error_code_strings[] = {
	{ AuthenticateErrorCode_invalidCertificate, "invalidCertificate" },
	{ AuthenticateErrorCode_invalidSignature, "invalidSignature" },
	{ AuthenticateErrorCode_unsupportedCurve, "unsupportedCurve" },
	{ AuthenticateErrorCode_noSessionContext, "noSessionContext" },
	{ AuthenticateErrorCode_invalidOid, "invalidOid" },
	{ AuthenticateErrorCode_euiccChallengeMismatch, "euiccChallengeMismatch" },
	{ AuthenticateErrorCode_ciPKUnknown, "ciPKUnknown" },
	{ AuthenticateErrorCode_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_auth_serv_res(struct ipa_es10b_auth_serv_res *res, const struct ipa_buf *es10b_res)
{
	struct AuthenticateServerResponse *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_AuthenticateServerResponse, es10b_res, "AuthenticateServer");
	if (!asn)
		return -EINVAL;

	switch (asn->present) {
	case AuthenticateServerResponse_PR_authenticateResponseOk:
		res->auth_serv_ok = &asn->choice.authenticateResponseOk;
		break;
	case AuthenticateServerResponse_PR_authenticateResponseError:
		res->auth_serv_err = asn->choice.authenticateResponseError.authenticateErrorCode;
		IPA_LOGP_ES10X("AuthenticateServer", LERROR, "function failed with error code %ld=%s!\n",
			       res->auth_serv_err, ipa_str_from_num(error_code_strings, res->auth_serv_err,
								    "(unknown)"));
		break;
	default:
		IPA_LOGP_ES10X("AuthenticateServer", LERROR, "unexpected response content!\n");
		res->auth_serv_err = -1;
	}

	res->res = asn;
	return 0;
}

/*! Function (ES10b): AuthenticateServer.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10b_auth_serv_res *ipa_es10b_auth_serv(struct ipa_context *ctx, const struct ipa_es10b_auth_serv_req *req)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ipa_es10b_auth_serv_res *res = IPA_ALLOC_ZERO(struct ipa_es10b_auth_serv_res);
	int rc;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_AuthenticateServerRequest, &req->req, "AuthenticateServer");
	if (!es10b_req) {
		IPA_LOGP_ES10X("AuthenticateServer", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("AuthenticateServer", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_auth_serv_res(res, es10b_res);
	if (rc < 0)
		goto error;

	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return res;
error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	ipa_es10b_auth_serv_res_free(res);
	return NULL;
}

/*! Free results of function (ES10b): AuthenticateServer.
 *  \param[in] res pointer to function result. */
void ipa_es10b_auth_serv_res_free(struct ipa_es10b_auth_serv_res *res)
{
	IPA_ES10X_RES_FREE(asn_DEF_AuthenticateServerResponse, res);
}
