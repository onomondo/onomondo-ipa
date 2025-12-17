/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 * 
 * See also: GSMA SGP.22, section 5.7.5: Function (ES10b): PrepareDownload
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
#include "es10b_prep_dwnld.h"

static const struct num_str_map error_code_strings[] = {
	{ DownloadErrorCode_invalidCertificate, "invalidCertificate" },
	{ DownloadErrorCode_invalidSignature, "invalidSignature" },
	{ DownloadErrorCode_unsupportedCurve, "unsupportedCurve" },
	{ DownloadErrorCode_noSessionContext, "noSessionContext" },
	{ DownloadErrorCode_invalidTransactionId, "invalidTransactionId" },
	{ DownloadErrorCode_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_prep_dwnld_res(struct ipa_es10b_prep_dwnld_res *res, const struct ipa_buf *es10b_res)
{
	struct PrepareDownloadResponse *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_PrepareDownloadResponse, es10b_res, "PrepareDownload");
	if (!asn)
		return -EINVAL;

	switch (asn->present) {
	case PrepareDownloadResponse_PR_downloadResponseOk:
		res->prep_dwnld_ok = &asn->choice.downloadResponseOk;
		break;
	case PrepareDownloadResponse_PR_downloadResponseError:
		res->prep_dwnld_err = asn->choice.downloadResponseError.downloadErrorCode;
		IPA_LOGP_ES10X("PrepareDownload", LERROR, "function failed with error code %ld=%s!\n",
			       res->prep_dwnld_err, ipa_str_from_num(error_code_strings, res->prep_dwnld_err,
								     "(unknown)"));
		break;
	default:
		IPA_LOGP_ES10X("PrepareDownload", LERROR, "unexpected response content!\n");
		res->prep_dwnld_err = -1;
	}

	res->res = asn;
	return 0;
}

/*! Function (ES10b): PrepareDownload.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10b_prep_dwnld_res *ipa_es10b_prep_dwnld(struct ipa_context *ctx,
						      const struct ipa_es10b_prep_dwnld_req *req)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ipa_es10b_prep_dwnld_res *res = IPA_ALLOC_ZERO(struct ipa_es10b_prep_dwnld_res);
	int rc;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_PrepareDownloadRequest, &req->req, "PrepareDownload");
	if (!es10b_req) {
		IPA_LOGP_ES10X("PrepareDownload", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("PrepareDownload", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_prep_dwnld_res(res, es10b_res);
	if (rc < 0)
		goto error;

	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return res;
error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	ipa_es10b_prep_dwnld_res_free(res);
	return NULL;
}

/*! Free results of function (ES10b): PrepareDownload.
 *  \param[in] res pointer to function result. */
void ipa_es10b_prep_dwnld_res_free(struct ipa_es10b_prep_dwnld_res *res)
{
	IPA_ES10X_RES_FREE(asn_DEF_PrepareDownloadResponse, res);
}
