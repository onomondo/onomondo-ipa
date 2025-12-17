/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, 5.7.16: Function (ES10c): EnableProfile
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
#include "es10c_enable_prfle.h"

static const struct num_str_map error_code_strings[] = {
	{ EnableProfileResponse__enableResult_ok, "ok" },
	{ EnableProfileResponse__enableResult_iccidOrAidNotFound, "iccidOrAidNotFound" },
	{ EnableProfileResponse__enableResult_profileNotInDisabledState, "profileNotInDisabledState" },
	{ EnableProfileResponse__enableResult_disallowedByPolicy, "disallowedByPolicy" },
	{ EnableProfileResponse__enableResult_wrongProfileReenabling, "wrongProfileReenabling" },
	{ EnableProfileResponse__enableResult_catBusy, "catBusy" },
	{ EnableProfileResponse__enableResult_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_enable_prfle_res(struct ipa_es10c_enable_prfle_res *res, const struct ipa_buf *es10c_res)
{
	struct EnableProfileResponse *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_EnableProfileResponse, es10c_res, "EnableProfile");
	if (!asn)
		return -EINVAL;

	if (asn->enableResult != EnableProfileResponse__enableResult_ok) {
		IPA_LOGP_ES10X("EnableProfile", LERROR, "function failed with error code %ld=%s!\n",
			       asn->enableResult, ipa_str_from_num(error_code_strings, asn->enableResult, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("EnableProfile", LERROR, "function succeeded with status code %ld=%s!\n",
			       asn->enableResult, ipa_str_from_num(error_code_strings, asn->enableResult, "(unknown)"));
	}

	res->res = asn;
	return 0;
}

/*! Function (Es10c): EnableProfile.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10c_enable_prfle_res *ipa_es10c_enable_prfle(struct ipa_context *ctx,
							  const struct ipa_es10c_enable_prfle_req *req)
{
	struct ipa_buf *es10c_req = NULL;
	struct ipa_buf *es10c_res = NULL;
	struct ipa_es10c_enable_prfle_res *res = IPA_ALLOC_ZERO(struct ipa_es10c_enable_prfle_res);
	int rc;

	es10c_req = ipa_es10x_req_enc(&asn_DEF_EnableProfileRequest, &req->req, "EnableProfile");
	if (!es10c_req) {
		IPA_LOGP_ES10X("EnableProfile", LERROR, "unable to encode Es10c request\n");
		goto error;
	}

	es10c_res = ipa_euicc_transceive_es10x(ctx, es10c_req);
	if (!es10c_res) {
		IPA_LOGP_ES10X("EnableProfile", LERROR, "no Es10c response\n");
		goto error;
	}

	rc = dec_enable_prfle_res(res, es10c_res);
	if (rc < 0)
		goto error;

	IPA_FREE(es10c_req);
	IPA_FREE(es10c_res);
	return res;
error:
	IPA_FREE(es10c_req);
	IPA_FREE(es10c_res);
	ipa_es10c_enable_prfle_res_free(res);
	return NULL;
}

/*! Free results of function (Es10c): EnableProfile.
 *  \param[in] res pointer to function result. */
void ipa_es10c_enable_prfle_res_free(struct ipa_es10c_enable_prfle_res *res)
{
	IPA_ES10X_RES_FREE(asn_DEF_EnableProfileResponse, res);
}
