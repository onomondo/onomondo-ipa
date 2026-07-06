/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * See also: GSMA SGP.32 v1.2, 5.9.20: Function (ES10b): ExecuteFallbackMechanism
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <ExecuteFallbackMechanismRequest.h>
#include <ExecuteFallbackMechanismResponse.h>
#include "context.h"
#include "length.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_exec_fallback.h"
#include "es10c_get_prfle_info.h"
#include "es10c_enable_prfle.h"

static const struct num_str_map error_code_strings[] = {
	{ ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_ok, "ok" },
	{ ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_profileNotInDisabledState,
	  "profileNotInDisabledState" },
	{ ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_catBusy, "catBusy" },
	{ ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_fallbackNotAvailable,
	  "fallbackNotAvailable" },
	{ ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_commandError, "commandError" },
	{ ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_ecallActive, "ecallActive" },
	{ ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_undefinedError, "undefinedError" },
	{ 0, NULL }
};

/*! Compare an ASN.1 encoded ICCID against a raw ICCID as it is kept in the nvstate.
 *  \param[in] iccid ASN.1 ICCID (may be NULL).
 *  \param[in] raw raw ICCID.
 *  \param[in] raw_len length of the raw ICCID, 0 means "not set".
 *  \returns true when both are present and equal. */
bool iot_emu_iccid_eq(const Iccid_t *iccid, const uint8_t *raw, uint8_t raw_len)
{
	if (!iccid || raw_len == 0)
		return false;
	return iccid->size == raw_len && memcmp(iccid->buf, raw, raw_len) == 0;
}

/*! Check whether the profile holding the emulated fallback attribute still exists on the eUICC.
 *  On a real IoT eUICC the fallback attribute lives in the profile metadata and disappears together
 *  with its profile; a stored fallback ICCID that no longer matches any installed profile is
 *  therefore cleared and treated as "no fallback profile".
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] prfle_info result of a previous ipa_es10c_get_prfle_info call.
 *  \returns true when a fallback profile exists. */
bool iot_emu_fallback_prfle_exists(struct ipa_context *ctx, const struct ipa_es10c_get_prfle_info_res *prfle_info)
{
	int i;

	if (ctx->nvstate.iot_euicc_emu.fallback.fallback_iccid_len == 0)
		return false;

	if (prfle_info->sgp32_res && prfle_info->sgp32_res->present == SGP32_ProfileInfoListResponse_PR_profileInfoListOk) {
		for (i = 0; i < prfle_info->sgp32_res->choice.profileInfoListOk.list.count; i++) {
			if (iot_emu_iccid_eq(prfle_info->sgp32_res->choice.profileInfoListOk.list.array[i]->iccid,
					     ctx->nvstate.iot_euicc_emu.fallback.fallback_iccid,
					     ctx->nvstate.iot_euicc_emu.fallback.fallback_iccid_len))
				return true;
		}
	}

	ctx->nvstate.iot_euicc_emu.fallback.fallback_iccid_len = 0;
	return false;
}

static int dec_exec_fallback_res(const struct ipa_buf *es10b_res)
{
	struct ExecuteFallbackMechanismResponse *asn = NULL;
	int rc;

	asn = ipa_es10x_res_dec(&asn_DEF_ExecuteFallbackMechanismResponse, es10b_res, "ExecuteFallbackMechanism");
	if (!asn)
		return -EINVAL;

	rc = asn->executeFallbackMechanismResult;

	if (rc == ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_ok) {
		IPA_LOGP_ES10X("ExecuteFallbackMechanism", LERROR, "function succeeded with status code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("ExecuteFallbackMechanism", LERROR, "function failed with error code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	}

	ASN_STRUCT_FREE(asn_DEF_ExecuteFallbackMechanismResponse, asn);
	return rc;
}

int exec_fallback(struct ipa_context *ctx)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ExecuteFallbackMechanismRequest exec_fallback_req = { 0 };
	int rc = -EINVAL;

	exec_fallback_req.refreshFlag = ctx->cfg->refresh_flag;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_ExecuteFallbackMechanismRequest, &exec_fallback_req,
				      "ExecuteFallbackMechanism");
	if (!es10b_req) {
		IPA_LOGP_ES10X("ExecuteFallbackMechanism", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("ExecuteFallbackMechanism", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_exec_fallback_res(es10b_res);
	if (rc < 0)
		goto error;

error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return rc;
}

int exec_fallback_emu(struct ipa_context *ctx)
{
	struct ipa_es10c_get_prfle_info_res *prfle_info = NULL;
	struct ipa_es10c_enable_prfle_req enable_prfle_req = { 0 };
	struct ipa_es10c_enable_prfle_res *enable_prfle_res = NULL;
	const Iccid_t *enabled_iccid;
	int rc = ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_undefinedError;

	prfle_info = ipa_es10c_get_prfle_info(ctx, NULL);
	if (!prfle_info || prfle_info->prfle_info_list_err != 0)
		goto error;

	/* An enabled profile (with an ICCID we can return to) must be present */
	if (!prfle_info->currently_active_prfle || !prfle_info->currently_active_prfle->iccid ||
	    prfle_info->currently_active_prfle->iccid->size > IPA_LEN_ICCID) {
		rc = ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_commandError;
		goto error;
	}
	enabled_iccid = prfle_info->currently_active_prfle->iccid;

	/* A fallback profile must exist */
	if (!iot_emu_fallback_prfle_exists(ctx, prfle_info)) {
		rc = ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_fallbackNotAvailable;
		goto error;
	}

	/* A real eUICC would reject the request with ecallActive while the emergency profile is enabled.
	 * Under consumer eUICC emulation no emergency profile can exist (see es10b_enable_emergency_prfle.c),
	 * so this check is unreachable here. */

	/* The fallback profile must be in disabled state */
	if (iot_emu_iccid_eq(enabled_iccid, ctx->nvstate.iot_euicc_emu.fallback.fallback_iccid,
			     ctx->nvstate.iot_euicc_emu.fallback.fallback_iccid_len)) {
		rc = ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_profileNotInDisabledState;
		goto error;
	}

	/* Remember the profile to return to (see ReturnFromFallback), then enable the fallback profile.
	 * The SGP.22 EnableProfile implicitly disables the currently enabled profile. */
	memcpy(ctx->nvstate.iot_euicc_emu.fallback.return_iccid, enabled_iccid->buf, enabled_iccid->size);
	ctx->nvstate.iot_euicc_emu.fallback.return_iccid_len = enabled_iccid->size;

	enable_prfle_req.req.profileIdentifier.present = EnableProfileRequest__profileIdentifier_PR_iccid;
	enable_prfle_req.req.profileIdentifier.choice.iccid.buf = ctx->nvstate.iot_euicc_emu.fallback.fallback_iccid;
	enable_prfle_req.req.profileIdentifier.choice.iccid.size =
	    ctx->nvstate.iot_euicc_emu.fallback.fallback_iccid_len;
	enable_prfle_req.req.refreshFlag = ctx->cfg->refresh_flag;

	enable_prfle_res = ipa_es10c_enable_prfle(ctx, &enable_prfle_req);
	if (!enable_prfle_res) {
		rc = ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_undefinedError;
	} else {
		switch (enable_prfle_res->res->enableResult) {
		case EnableProfileResponse__enableResult_ok:
			rc = ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_ok;
			break;
		case EnableProfileResponse__enableResult_catBusy:
			rc = ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_catBusy;
			break;
		case EnableProfileResponse__enableResult_iccidOrAidNotFound:
			rc = ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_fallbackNotAvailable;
			break;
		default:
			rc = ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_undefinedError;
			break;
		}
	}

	/* The function is atomic: no profile switch, nothing to return from */
	if (rc != ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_ok)
		ctx->nvstate.iot_euicc_emu.fallback.return_iccid_len = 0;

error:
	if (rc == ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_ok) {
		IPA_LOGP_ES10X("ExecuteFallbackMechanism", LERROR,
			       "IoT eUICC emulation active, function succeeded with status code %d=%s!\n", rc,
			       ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("ExecuteFallbackMechanism", LERROR,
			       "IoT eUICC emulation active, function failed with error code %d=%s!\n", rc,
			       ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	}

	ipa_es10c_enable_prfle_res_free(enable_prfle_res);
	ipa_es10c_get_prfle_info_res_free(prfle_info);
	return rc;
}

/*! Function (ES10b): ExecuteFallbackMechanism.
 *  \param[inout] ctx pointer to ipa_context.
 *  \returns positive status code on success, negative on error. */
int ipa_es10b_exec_fallback(struct ipa_context *ctx)
{
	if (ctx->cfg->iot_euicc_emu_enabled)
		return exec_fallback_emu(ctx);
	else
		return exec_fallback(ctx);
}
