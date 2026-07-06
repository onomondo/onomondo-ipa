/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * See also: GSMA SGP.32 v1.2, 5.9.23: Function (ES10b): DisableEmergencyProfile
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <DisableEmergencyProfileRequest.h>
#include <DisableEmergencyProfileResponse.h>
#include "context.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_disable_emergency_prfle.h"

static const struct num_str_map error_code_strings[] = {
	{ DisableEmergencyProfileResponse__disableEmergencyProfileResult_ok, "ok" },
	{ DisableEmergencyProfileResponse__disableEmergencyProfileResult_profileNotInEnabledState,
	  "profileNotInEnabledState" },
	{ DisableEmergencyProfileResponse__disableEmergencyProfileResult_catBusy, "catBusy" },
	{ DisableEmergencyProfileResponse__disableEmergencyProfileResult_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_disable_emergency_prfle_res(const struct ipa_buf *es10b_res)
{
	struct DisableEmergencyProfileResponse *asn = NULL;
	int rc;

	asn = ipa_es10x_res_dec(&asn_DEF_DisableEmergencyProfileResponse, es10b_res, "DisableEmergencyProfile");
	if (!asn)
		return -EINVAL;

	rc = asn->disableEmergencyProfileResult;

	if (rc == DisableEmergencyProfileResponse__disableEmergencyProfileResult_ok) {
		IPA_LOGP_ES10X("DisableEmergencyProfile", LERROR, "function succeeded with status code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("DisableEmergencyProfile", LERROR, "function failed with error code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	}

	ASN_STRUCT_FREE(asn_DEF_DisableEmergencyProfileResponse, asn);
	return rc;
}

int disable_emergency_prfle(struct ipa_context *ctx)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct DisableEmergencyProfileRequest disable_emergency_req = { 0 };
	int rc = -EINVAL;

	disable_emergency_req.refreshFlag = ctx->cfg->refresh_flag;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_DisableEmergencyProfileRequest, &disable_emergency_req,
				      "DisableEmergencyProfile");
	if (!es10b_req) {
		IPA_LOGP_ES10X("DisableEmergencyProfile", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("DisableEmergencyProfile", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_disable_emergency_prfle_res(es10b_res);
	if (rc < 0)
		goto error;

error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return rc;
}

int disable_emergency_prfle_emu(struct ipa_context *ctx)
{
	(void)ctx;

	/* No emergency profile can exist on an emulated IoT eUICC (see es10b_enable_emergency_prfle.c), so
	 * it can never be in enabled state. The result set of this function has no ecallNotAvailable, the
	 * truthful precondition failure is profileNotInEnabledState. */
	IPA_LOGP_ES10X("DisableEmergencyProfile", LERROR,
		       "IoT eUICC emulation active, function failed with error code %d=%s!\n",
		       (int)DisableEmergencyProfileResponse__disableEmergencyProfileResult_profileNotInEnabledState,
		       ipa_str_from_num(error_code_strings,
					DisableEmergencyProfileResponse__disableEmergencyProfileResult_profileNotInEnabledState,
					"(unknown)"));
	return DisableEmergencyProfileResponse__disableEmergencyProfileResult_profileNotInEnabledState;
}

/*! Function (ES10b): DisableEmergencyProfile.
 *  \param[inout] ctx pointer to ipa_context.
 *  \returns positive status code on success, negative on error. */
int ipa_es10b_disable_emergency_prfle(struct ipa_context *ctx)
{
	if (ctx->cfg->iot_euicc_emu_enabled)
		return disable_emergency_prfle_emu(ctx);
	else
		return disable_emergency_prfle(ctx);
}
