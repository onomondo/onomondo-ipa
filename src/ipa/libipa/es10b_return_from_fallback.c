/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * See also: GSMA SGP.32 v1.2, 5.9.21: Function (ES10b): ReturnFromFallback
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <ReturnFromFallbackRequest.h>
#include <ReturnFromFallbackResponse.h>
#include "context.h"
#include "length.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_return_from_fallback.h"
#include "es10b_exec_fallback.h"
#include "es10c_get_prfle_info.h"
#include "es10c_enable_prfle.h"

static const struct num_str_map error_code_strings[] = {
	{ ReturnFromFallbackResponse__returnFromFallbackResult_ok, "ok" },
	{ ReturnFromFallbackResponse__returnFromFallbackResult_catBusy, "catBusy" },
	{ ReturnFromFallbackResponse__returnFromFallbackResult_fallbackNotAvailable, "fallbackNotAvailable" },
	{ ReturnFromFallbackResponse__returnFromFallbackResult_commandError, "commandError" },
	{ ReturnFromFallbackResponse__returnFromFallbackResult_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_return_from_fallback_res(const struct ipa_buf *es10b_res)
{
	struct ReturnFromFallbackResponse *asn = NULL;
	int rc;

	asn = ipa_es10x_res_dec(&asn_DEF_ReturnFromFallbackResponse, es10b_res, "ReturnFromFallback");
	if (!asn)
		return -EINVAL;

	rc = asn->returnFromFallbackResult;

	if (rc == ReturnFromFallbackResponse__returnFromFallbackResult_ok) {
		IPA_LOGP_ES10X("ReturnFromFallback", LERROR, "function succeeded with status code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("ReturnFromFallback", LERROR, "function failed with error code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	}

	ASN_STRUCT_FREE(asn_DEF_ReturnFromFallbackResponse, asn);
	return rc;
}

int return_from_fallback(struct ipa_context *ctx)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ReturnFromFallbackRequest return_from_fallback_req = { 0 };
	int rc = -EINVAL;

	return_from_fallback_req.refreshFlag = ctx->cfg->refresh_flag;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_ReturnFromFallbackRequest, &return_from_fallback_req,
				      "ReturnFromFallback");
	if (!es10b_req) {
		IPA_LOGP_ES10X("ReturnFromFallback", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("ReturnFromFallback", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_return_from_fallback_res(es10b_res);
	if (rc < 0)
		goto error;

error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return rc;
}

int return_from_fallback_emu(struct ipa_context *ctx)
{
	struct ipa_es10c_get_prfle_info_res *prfle_info = NULL;
	struct ipa_es10c_enable_prfle_req enable_prfle_req = { 0 };
	struct ipa_es10c_enable_prfle_res *enable_prfle_res = NULL;
	int rc = ReturnFromFallbackResponse__returnFromFallbackResult_undefinedError;

	prfle_info = ipa_es10c_get_prfle_info(ctx, NULL);
	if (!prfle_info || prfle_info->prfle_info_list_err != 0)
		goto error;

	/* The currently enabled profile must be the fallback profile */
	if (!iot_emu_fallback_prfle_exists(ctx, prfle_info) ||
	    !prfle_info->currently_active_prfle ||
	    !iot_emu_iccid_eq(prfle_info->currently_active_prfle->iccid,
			      ctx->nvstate.iot_euicc_emu.fallback.fallback_iccid,
			      ctx->nvstate.iot_euicc_emu.fallback.fallback_iccid_len)) {
		rc = ReturnFromFallbackResponse__returnFromFallbackResult_fallbackNotAvailable;
		goto error;
	}

	/* Without a recorded previously enabled profile there is nothing to return to */
	if (ctx->nvstate.iot_euicc_emu.fallback.return_iccid_len == 0) {
		rc = ReturnFromFallbackResponse__returnFromFallbackResult_commandError;
		goto error;
	}

	/* Re-enable the profile that was enabled before the fallback (the SGP.22 EnableProfile
	 * implicitly disables the currently enabled fallback profile). */
	enable_prfle_req.req.profileIdentifier.present = EnableProfileRequest__profileIdentifier_PR_iccid;
	enable_prfle_req.req.profileIdentifier.choice.iccid.buf = ctx->nvstate.iot_euicc_emu.fallback.return_iccid;
	enable_prfle_req.req.profileIdentifier.choice.iccid.size =
	    ctx->nvstate.iot_euicc_emu.fallback.return_iccid_len;
	enable_prfle_req.req.refreshFlag = ctx->cfg->refresh_flag;

	enable_prfle_res = ipa_es10c_enable_prfle(ctx, &enable_prfle_req);
	if (!enable_prfle_res) {
		rc = ReturnFromFallbackResponse__returnFromFallbackResult_undefinedError;
	} else {
		switch (enable_prfle_res->res->enableResult) {
		case EnableProfileResponse__enableResult_ok:
			rc = ReturnFromFallbackResponse__returnFromFallbackResult_ok;
			ctx->nvstate.iot_euicc_emu.fallback.return_iccid_len = 0;
			break;
		case EnableProfileResponse__enableResult_catBusy:
			rc = ReturnFromFallbackResponse__returnFromFallbackResult_catBusy;
			break;
		default:
			rc = ReturnFromFallbackResponse__returnFromFallbackResult_undefinedError;
			break;
		}
	}

error:
	if (rc == ReturnFromFallbackResponse__returnFromFallbackResult_ok) {
		IPA_LOGP_ES10X("ReturnFromFallback", LERROR,
			       "IoT eUICC emulation active, function succeeded with status code %d=%s!\n", rc,
			       ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("ReturnFromFallback", LERROR,
			       "IoT eUICC emulation active, function failed with error code %d=%s!\n", rc,
			       ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	}

	ipa_es10c_enable_prfle_res_free(enable_prfle_res);
	ipa_es10c_get_prfle_info_res_free(prfle_info);
	return rc;
}

/*! Function (ES10b): ReturnFromFallback.
 *  \param[inout] ctx pointer to ipa_context.
 *  \returns positive status code on success, negative on error. */
int ipa_es10b_return_from_fallback(struct ipa_context *ctx)
{
	if (ctx->cfg->iot_euicc_emu_enabled)
		return return_from_fallback_emu(ctx);
	else
		return return_from_fallback(ctx);
}
