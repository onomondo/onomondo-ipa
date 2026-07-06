/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * See also: GSMA SGP.32 v1.2, 5.9.24: Function (ES10b): GetConnectivityParameters
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <GetConnectivityParametersRequest.h>
#include <GetConnectivityParametersResponse.h>
#include "context.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_get_conn_params.h"

static const struct num_str_map error_code_strings[] = {
	{ ConnectivityParametersError_parametersNotAvailable, "parametersNotAvailable" },
	{ ConnectivityParametersError_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_get_conn_params_res(const struct ipa_buf *es10b_res, struct ipa_buf **http_params)
{
	struct GetConnectivityParametersResponse *asn = NULL;
	int rc;

	asn = ipa_es10x_res_dec(&asn_DEF_GetConnectivityParametersResponse, es10b_res, "GetConnectivityParameters");
	if (!asn)
		return -EINVAL;

	switch (asn->present) {
	case GetConnectivityParametersResponse_PR_connectivityParameters:
		/* httpParams (also used for CoAP) is optional, a profile may carry an empty parameter set */
		if (asn->choice.connectivityParameters.httpParams)
			*http_params = IPA_BUF_FROM_ASN(asn->choice.connectivityParameters.httpParams);
		IPA_LOGP_ES10X("GetConnectivityParameters", LERROR, "function succeeded, %s connectivity parameters\n",
			       *http_params ? "received" : "no");
		rc = 0;
		break;
	case GetConnectivityParametersResponse_PR_connectivityParametersError:
		rc = asn->choice.connectivityParametersError;
		IPA_LOGP_ES10X("GetConnectivityParameters", LERROR, "function failed with error code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
		break;
	default:
		IPA_LOGP_ES10X("GetConnectivityParameters", LERROR, "unexpected response!\n");
		rc = -EINVAL;
		break;
	}

	ASN_STRUCT_FREE(asn_DEF_GetConnectivityParametersResponse, asn);
	return rc;
}

int get_conn_params(struct ipa_context *ctx, struct ipa_buf **http_params)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct GetConnectivityParametersRequest get_conn_params_req = { 0 };
	int rc = -EINVAL;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_GetConnectivityParametersRequest, &get_conn_params_req,
				      "GetConnectivityParameters");
	if (!es10b_req) {
		IPA_LOGP_ES10X("GetConnectivityParameters", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("GetConnectivityParameters", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_get_conn_params_res(es10b_res, http_params);
	if (rc < 0)
		goto error;

error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return rc;
}

int get_conn_params_emu(struct ipa_context *ctx)
{
	(void)ctx;

	/* The connectivity parameters live in the profile file structure of an IoT profile (SGP.02, table 95)
	 * and are read out by the ISD-R. Consumer ES10x offers no function to access them, so under emulation
	 * they are genuinely not available. */
	IPA_LOGP_ES10X("GetConnectivityParameters", LERROR,
		       "IoT eUICC emulation active, function failed with error code %d=%s!\n",
		       (int)ConnectivityParametersError_parametersNotAvailable,
		       ipa_str_from_num(error_code_strings, ConnectivityParametersError_parametersNotAvailable,
					"(unknown)"));
	return ConnectivityParametersError_parametersNotAvailable;
}

/*! Function (ES10b): GetConnectivityParameters.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[out] http_params HTTP/CoAP connectivity parameters of the enabled profile (heap copy, the
 *              caller must free it). Set to NULL when the profile carries no parameters.
 *  \returns 0 on success, positive status code or negative on error. */
int ipa_es10b_get_conn_params(struct ipa_context *ctx, struct ipa_buf **http_params)
{
	*http_params = NULL;

	if (ctx->cfg->iot_euicc_emu_enabled)
		return get_conn_params_emu(ctx);
	else
		return get_conn_params(ctx, http_params);
}
