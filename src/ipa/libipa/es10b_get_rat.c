/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, 5.7.22: Function (ES10b): GetRAT
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <GetRatRequest.h>
#include "context.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_get_rat.h"

static int dec_get_rat_res(struct ipa_es10b_get_rat_res *res, const struct ipa_buf *es10b_res)
{
	struct GetRatResponse *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_GetRatResponse, es10b_res, "GetRAT");
	if (!asn)
		return -EINVAL;

	res->res = asn;
	return 0;
}

/*! Function (ES10b): GetRAT.
 *  \param[inout] ctx pointer to ipa_context.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10b_get_rat_res *ipa_es10b_get_rat(struct ipa_context *ctx)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	const struct GetRatRequest req = { 0 };
	struct ipa_es10b_get_rat_res *res = IPA_ALLOC_ZERO(struct ipa_es10b_get_rat_res);
	int rc;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_GetRatRequest, &req, "GetRAT");
	if (!es10b_req) {
		IPA_LOGP_ES10X("GetRAT", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("GetRAT", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_get_rat_res(res, es10b_res);
	if (rc < 0)
		goto error;

	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return res;
error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	ipa_es10b_get_rat_res_free(res);
	return NULL;
}

/*! Free results of function (ES10b): GetRAT.
 *  \param[in] res pointer to function result. */
void ipa_es10b_get_rat_res_free(struct ipa_es10b_get_rat_res *res)
{
	IPA_ES10X_RES_FREE(asn_DEF_GetRatResponse, res);
}
