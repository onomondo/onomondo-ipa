/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 * See also: GSMA SGP.32, 5.9.10: Function (ES10b): GetCerts
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
#include "es10b_get_certs.h"

static const struct num_str_map error_code_strings[] = {
	{ GetCertsResponse__getCertsError_invalidCiPKId, "invalidCiPKId" },
	{ GetCertsResponse__getCertsError_undfinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_get_certs_res(struct ipa_es10b_get_certs_res *res, const struct ipa_buf *es10b_res)
{
	struct GetCertsResponse *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_GetCertsResponse, es10b_res, "GetCerts");
	if (!asn)
		return -EINVAL;

	switch (asn->present) {
	case GetCertsResponse_PR_certs:
		res->eum_certificate = &asn->choice.certs.eumCertificate;
		res->euicc_certificate = &asn->choice.certs.euiccCertificate;
		break;
	case GetCertsResponse_PR_getCertsError:
		res->get_certs_err = asn->choice.getCertsError;
		IPA_LOGP_ES10X("GetCerts", LERROR, "function failed with error code %ld=%s!\n",
			       res->get_certs_err, ipa_str_from_num(error_code_strings, res->get_certs_err,
								    "(unknown)"));
		break;
	default:
		IPA_LOGP_ES10X("GetCerts", LERROR, "unexpected response content!\n");
		res->get_certs_err = -1;
	}

	res->res = asn;
	return 0;
}

/*! Function (ES10b): GetCerts.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10b_get_certs_res *ipa_es10b_get_certs(struct ipa_context *ctx, const struct ipa_es10b_get_certs_req *req)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ipa_es10b_get_certs_res *res = IPA_ALLOC_ZERO(struct ipa_es10b_get_certs_res);
	int rc;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_GetCertsRequest, &req->req, "GetCerts");
	if (!es10b_req) {
		IPA_LOGP_ES10X("GetCerts", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("GetCerts", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_get_certs_res(res, es10b_res);
	if (rc < 0)
		goto error;

	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return res;
error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	ipa_es10b_get_certs_res_free(res);
	return NULL;
}

/*! Free results of function (ES10b): GetCerts.
 *  \param[in] res pointer to function result. */
void ipa_es10b_get_certs_res_free(struct ipa_es10b_get_certs_res *res)
{
	IPA_ES10X_RES_FREE(asn_DEF_GetCertsResponse, res);
}
