/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, 5.7.18: Function (ES10c): DeleteProfile
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
#include "es10c_delete_prfle.h"

static const struct num_str_map error_code_strings[] = {
	{ DeleteProfileResponse__deleteResult_ok, "ok" },
	{ DeleteProfileResponse__deleteResult_iccidOrAidNotFound, "iccidOrAidNotFound" },
	{ DeleteProfileResponse__deleteResult_profileNotInDisabledState, "profileNotInDisabledState" },
	{ DeleteProfileResponse__deleteResult_disallowedByPolicy, "disallowedByPolicy" },
	{ DeleteProfileResponse__deleteResult_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_delete_prfle_res(struct ipa_es10c_delete_prfle_res *res, const struct ipa_buf *es10c_res)
{
	struct DeleteProfileResponse *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_DeleteProfileResponse, es10c_res, "DeleteProfile");
	if (!asn)
		return -EINVAL;

	if (asn->deleteResult != DeleteProfileResponse__deleteResult_ok) {
		IPA_LOGP_ES10X("DeleteProfile", LERROR, "function failed with error code %ld=%s!\n",
			       asn->deleteResult, ipa_str_from_num(error_code_strings, asn->deleteResult, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("DeleteProfile", LERROR, "function succeeded with status code %ld=%s!\n",
			       asn->deleteResult, ipa_str_from_num(error_code_strings, asn->deleteResult, "(unknown)"));
	}

	res->res = asn;
	return 0;
}

/*! Function (Es10c): DeleteProfile.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10c_delete_prfle_res *ipa_es10c_delete_prfle(struct ipa_context *ctx,
							  const struct ipa_es10c_delete_prfle_req *req)
{
	struct ipa_buf *es10c_req = NULL;
	struct ipa_buf *es10c_res = NULL;
	struct ipa_es10c_delete_prfle_res *res = IPA_ALLOC_ZERO(struct ipa_es10c_delete_prfle_res);
	int rc;

	es10c_req = ipa_es10x_req_enc(&asn_DEF_DeleteProfileRequest, &req->req, "DeleteProfile");
	if (!es10c_req) {
		IPA_LOGP_ES10X("DeleteProfile", LERROR, "unable to encode Es10c request\n");
		goto error;
	}

	es10c_res = ipa_euicc_transceive_es10x(ctx, es10c_req);
	if (!es10c_res) {
		IPA_LOGP_ES10X("DeleteProfile", LERROR, "no Es10c response\n");
		goto error;
	}

	rc = dec_delete_prfle_res(res, es10c_res);
	if (rc < 0)
		goto error;

	IPA_FREE(es10c_req);
	IPA_FREE(es10c_res);
	return res;
error:
	IPA_FREE(es10c_req);
	IPA_FREE(es10c_res);
	ipa_es10c_delete_prfle_res_free(res);
	return NULL;
}

/*! Free results of function (Es10c): DeleteProfile.
 *  \param[in] res pointer to function result. */
void ipa_es10c_delete_prfle_res_free(struct ipa_es10c_delete_prfle_res *res)
{
	IPA_ES10X_RES_FREE(asn_DEF_DeleteProfileResponse, res);
}
