/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * See also: GSMA SGP.32 v1.2, 5.9.22: Function (ES10b): EnableEmergencyProfile
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <EnableEmergencyProfileRequest.h>
#include <EnableEmergencyProfileResponse.h>
#include "context.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_enable_emergency_prfle.h"

static const struct num_str_map error_code_strings[] = {
	{ EnableEmergencyProfileResponse__enableEmergencyProfileResult_ok, "ok" },
	{ EnableEmergencyProfileResponse__enableEmergencyProfileResult_profileNotInDisabledState,
	  "profileNotInDisabledState" },
	{ EnableEmergencyProfileResponse__enableEmergencyProfileResult_catBusy, "catBusy" },
	{ EnableEmergencyProfileResponse__enableEmergencyProfileResult_ecallNotAvailable, "ecallNotAvailable" },
	{ EnableEmergencyProfileResponse__enableEmergencyProfileResult_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_enable_emergency_prfle_res(const struct ipa_buf *es10b_res)
{
	struct EnableEmergencyProfileResponse *asn = NULL;
	int rc;

	asn = ipa_es10x_res_dec(&asn_DEF_EnableEmergencyProfileResponse, es10b_res, "EnableEmergencyProfile");
	if (!asn)
		return -EINVAL;

	rc = asn->enableEmergencyProfileResult;

	if (rc == EnableEmergencyProfileResponse__enableEmergencyProfileResult_ok) {
		IPA_LOGP_ES10X("EnableEmergencyProfile", LERROR, "function succeeded with status code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("EnableEmergencyProfile", LERROR, "function failed with error code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	}

	ASN_STRUCT_FREE(asn_DEF_EnableEmergencyProfileResponse, asn);
	return rc;
}

int enable_emergency_prfle(struct ipa_context *ctx)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct EnableEmergencyProfileRequest enable_emergency_req = { 0 };
	int rc = -EINVAL;

	enable_emergency_req.refreshFlag = ctx->cfg->refresh_flag;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_EnableEmergencyProfileRequest, &enable_emergency_req,
				      "EnableEmergencyProfile");
	if (!es10b_req) {
		IPA_LOGP_ES10X("EnableEmergencyProfile", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("EnableEmergencyProfile", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_enable_emergency_prfle_res(es10b_res);
	if (rc < 0)
		goto error;

error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return rc;
}

int enable_emergency_prfle_emu(struct ipa_context *ctx)
{
	(void)ctx;

	/* An emergency profile is the (single) profile whose metadata carries an ecallIndication. That
	 * indication can only be set at install time via the SGP.32 StoreMetadataRequest; consumer (SGP.22)
	 * profile metadata has no such field, so no emergency profile can exist on an emulated IoT eUICC.
	 * ecallNotAvailable is exactly what a real eUICC without an emergency profile returns. */
	IPA_LOGP_ES10X("EnableEmergencyProfile", LERROR,
		       "IoT eUICC emulation active, function failed with error code %d=%s!\n",
		       (int)EnableEmergencyProfileResponse__enableEmergencyProfileResult_ecallNotAvailable,
		       ipa_str_from_num(error_code_strings,
					EnableEmergencyProfileResponse__enableEmergencyProfileResult_ecallNotAvailable,
					"(unknown)"));
	return EnableEmergencyProfileResponse__enableEmergencyProfileResult_ecallNotAvailable;
}

/*! Function (ES10b): EnableEmergencyProfile.
 *  \param[inout] ctx pointer to ipa_context.
 *  \returns positive status code on success, negative on error. */
int ipa_es10b_enable_emergency_prfle(struct ipa_context *ctx)
{
	if (ctx->cfg->iot_euicc_emu_enabled)
		return enable_emergency_prfle_emu(ctx);
	else
		return enable_emergency_prfle(ctx);
}
