/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.32 v1.2, 5.9.15: Function (ES10b): ImmediateEnable (EnableUsingDD before v1.2)
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <ImmediateEnableRequest.h>
#include <ImmediateEnableResponse.h>
#include "context.h"
#include "length.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_enable_using_dd.h"
#include "es10c_enable_prfle.h"

static const struct num_str_map error_code_strings[] = {
	{ ImmediateEnableResponse__immediateEnableResult_ok, "ok" },
	{ ImmediateEnableResponse__immediateEnableResult_immediateEnableNotAvailable, "immediateEnableNotAvailable" },
	{ ImmediateEnableResponse__immediateEnableResult_noSessionContext, "noSessionContext" },
	{ ImmediateEnableResponse__immediateEnableResult_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_enable_using_dd_res(const struct ipa_buf *es10b_res)
{
	struct ImmediateEnableResponse *asn = NULL;
	int rc;

	asn = ipa_es10x_res_dec(&asn_DEF_ImmediateEnableResponse, es10b_res, "ImmediateEnable");
	if (!asn)
		return -EINVAL;

	rc = asn->immediateEnableResult;

	if (rc == ImmediateEnableResponse__immediateEnableResult_ok) {
		IPA_LOGP_ES10X("ImmediateEnable", LERROR, "function succeeded with status code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("ImmediateEnable", LERROR, "function failed with error code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	}

	ASN_STRUCT_FREE(asn_DEF_ImmediateEnableResponse, asn);
	return rc;
}

int enable_using_dd(struct ipa_context *ctx)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ImmediateEnableRequest enable_using_dd_req = { 0 };
	int rc = -EINVAL;

	/* Request an UICC REFRESH so the modem picks up the newly enabled profile */
	enable_using_dd_req.refreshFlag = 0xff;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_ImmediateEnableRequest, &enable_using_dd_req, "ImmediateEnable");
	if (!es10b_req) {
		IPA_LOGP_ES10X("ImmediateEnable", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("ImmediateEnable", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_enable_using_dd_res(es10b_res);
	if (rc < 0)
		goto error;

error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return rc;
}

int enable_using_dd_emu(struct ipa_context *ctx)
{
	struct ipa_es10c_enable_prfle_req enable_prfle_req = { 0 };
	struct ipa_es10c_enable_prfle_res *enable_prfle_res = NULL;
	int rc = ImmediateEnableResponse__immediateEnableResult_undefinedError;

	/* Auto enable must be active */
	if (ctx->nvstate.iot_euicc_emu.auto_enable.flag == false) {
		rc = ImmediateEnableResponse__immediateEnableResult_immediateEnableNotAvailable;
		goto error;
	}

	/* We also need either the smdp_oid or the smdp_address to verify against */
	if (ctx->nvstate.iot_euicc_emu.auto_enable.smdp_oid == NULL &&
	    ctx->nvstate.iot_euicc_emu.auto_enable.smdp_address == NULL) {
		rc = ImmediateEnableResponse__immediateEnableResult_immediateEnableNotAvailable;
		goto error;
	}

	/* Ensure the session context is present and complete */
	if (ctx->iot_euicc_emu.auto_enable.smdp_address == NULL) {
		rc = ImmediateEnableResponse__immediateEnableResult_noSessionContext;
		goto error;
	}
	if (ctx->iot_euicc_emu.auto_enable.smdp_oid == NULL) {
		rc = ImmediateEnableResponse__immediateEnableResult_noSessionContext;
		goto error;
	}
	if (ctx->iot_euicc_emu.auto_enable.profile_aid == NULL) {
		rc = ImmediateEnableResponse__immediateEnableResult_noSessionContext;
		goto error;
	}

	/* Verify smdp OID (if configured) */
	if (ctx->nvstate.iot_euicc_emu.auto_enable.smdp_oid) {
		if (ctx->nvstate.iot_euicc_emu.auto_enable.smdp_oid->len !=
		    ctx->iot_euicc_emu.auto_enable.smdp_oid->len) {
			rc = ImmediateEnableResponse__immediateEnableResult_immediateEnableNotAvailable;
			goto error;
		}
		if (memcmp(ctx->nvstate.iot_euicc_emu.auto_enable.smdp_oid->data,
			   ctx->iot_euicc_emu.auto_enable.smdp_oid->data,
			   ctx->iot_euicc_emu.auto_enable.smdp_oid->len)) {
			rc = ImmediateEnableResponse__immediateEnableResult_immediateEnableNotAvailable;
			goto error;
		}
	}

	/* Verify smdp address (if configured) */
	if (ctx->nvstate.iot_euicc_emu.auto_enable.smdp_address) {
		if (ctx->nvstate.iot_euicc_emu.auto_enable.smdp_address->len !=
		    ctx->iot_euicc_emu.auto_enable.smdp_address->len) {
			rc = ImmediateEnableResponse__immediateEnableResult_immediateEnableNotAvailable;
			goto error;
		}
		if (memcmp(ctx->nvstate.iot_euicc_emu.auto_enable.smdp_address->data,
			   ctx->iot_euicc_emu.auto_enable.smdp_address->data,
			   ctx->iot_euicc_emu.auto_enable.smdp_address->len)) {
			rc = ImmediateEnableResponse__immediateEnableResult_immediateEnableNotAvailable;
			goto error;
		}
	}

	/* Enable profile */
	enable_prfle_req.req.profileIdentifier.present = EnableProfileRequest__profileIdentifier_PR_isdpAid;
	IPA_ASSIGN_BUF_TO_ASN(enable_prfle_req.req.profileIdentifier.choice.isdpAid,
			      ctx->iot_euicc_emu.auto_enable.profile_aid->data,
			      ctx->iot_euicc_emu.auto_enable.profile_aid->len);

	enable_prfle_res = ipa_es10c_enable_prfle(ctx, &enable_prfle_req);
	if (enable_prfle_res && enable_prfle_res->res->enableResult == EnableProfileResponse__enableResult_ok)
		rc = ImmediateEnableResponse__immediateEnableResult_ok;
	else
		rc = ImmediateEnableResponse__immediateEnableResult_undefinedError;

error:
	if (rc == ImmediateEnableResponse__immediateEnableResult_ok) {
		IPA_LOGP_ES10X("ImmediateEnable", LERROR,
			       "IoT eUICC emulation active, function succeeded with status code %d=%s!\n", rc,
			       ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("ImmediateEnable", LERROR,
			       "IoT eUICC emulation active, function failed with error code %d=%s!\n", rc,
			       ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	}

	/* Ensure the auto enable data is cleared after use */
	ipa_buf_free(ctx->iot_euicc_emu.auto_enable.smdp_oid);
	ctx->iot_euicc_emu.auto_enable.smdp_oid = NULL;
	ipa_buf_free(ctx->iot_euicc_emu.auto_enable.smdp_address);
	ctx->iot_euicc_emu.auto_enable.smdp_address = NULL;
	ipa_buf_free(ctx->iot_euicc_emu.auto_enable.profile_aid);
	ctx->iot_euicc_emu.auto_enable.profile_aid = NULL;

	ipa_es10c_enable_prfle_res_free(enable_prfle_res);
	return rc;
}

/*! Function (ES10b): EnableUsingDD.
 *  \param[inout] ctx pointer to ipa_context.
 *  \returns positive status code on success, negative on error. */
int ipa_es10b_enable_using_dd(struct ipa_context *ctx)
{
	if (ctx->cfg->iot_euicc_emu_enabled)
		return enable_using_dd_emu(ctx);
	else
		return enable_using_dd(ctx);
}
