/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * See also: GSMA SGP.32 v1.2, 5.9.17: Function (ES10b): ConfigureImmediateProfileEnabling
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <ConfigureImmediateProfileEnablingRequest.h>
#include <ConfigureImmediateProfileEnablingResponse.h>
#include "context.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_cfg_immediate_enable.h"
#include "es10b_get_eim_cfg_data.h"

static const struct num_str_map error_code_strings[] = {
	{ ConfigureImmediateProfileEnablingResponse__configImmediateEnableResult_ok, "ok" },
	{ ConfigureImmediateProfileEnablingResponse__configImmediateEnableResult_insufficientMemory,
	  "insufficientMemory" },
	{ ConfigureImmediateProfileEnablingResponse__configImmediateEnableResult_associatedEimAlreadyExists,
	  "associatedEimAlreadyExists" },
	{ ConfigureImmediateProfileEnablingResponse__configImmediateEnableResult_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_cfg_immediate_enable_res(const struct ipa_buf *es10b_res)
{
	struct ConfigureImmediateProfileEnablingResponse *asn = NULL;
	int rc;

	asn = ipa_es10x_res_dec(&asn_DEF_ConfigureImmediateProfileEnablingResponse, es10b_res,
				"ConfigureImmediateProfileEnabling");
	if (!asn)
		return -EINVAL;

	rc = asn->configImmediateEnableResult;

	if (rc == ConfigureImmediateProfileEnablingResponse__configImmediateEnableResult_ok) {
		IPA_LOGP_ES10X("ConfigureImmediateProfileEnabling", LERROR,
			       "function succeeded with status code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("ConfigureImmediateProfileEnabling", LERROR,
			       "function failed with error code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	}

	ASN_STRUCT_FREE(asn_DEF_ConfigureImmediateProfileEnablingResponse, asn);
	return rc;
}

int cfg_immediate_enable(struct ipa_context *ctx, const struct ipa_es10b_cfg_immediate_enable_req *req)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ConfigureImmediateProfileEnablingRequest cfg_req = { 0 };
	NULL_t enable_flag = 0;
	OBJECT_IDENTIFIER_t smdp_oid = { 0 };
	UTF8String_t smdp_address = { 0 };
	int rc = -EINVAL;

	if (req->immediate_enable_flag)
		cfg_req.immediateEnableFlag = &enable_flag;
	if (req->smdp_oid) {
		smdp_oid.buf = req->smdp_oid->data;
		smdp_oid.size = req->smdp_oid->len;
		cfg_req.defaultSmdpOid = &smdp_oid;
	}
	if (req->smdp_address) {
		smdp_address.buf = (uint8_t *)req->smdp_address;
		smdp_address.size = strlen(req->smdp_address);
		cfg_req.defaultSmdpAddress = &smdp_address;
	}

	es10b_req = ipa_es10x_req_enc(&asn_DEF_ConfigureImmediateProfileEnablingRequest, &cfg_req,
				      "ConfigureImmediateProfileEnabling");
	if (!es10b_req) {
		IPA_LOGP_ES10X("ConfigureImmediateProfileEnabling", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("ConfigureImmediateProfileEnabling", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_cfg_immediate_enable_res(es10b_res);
	if (rc < 0)
		goto error;

error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return rc;
}

int cfg_immediate_enable_emu(struct ipa_context *ctx, const struct ipa_es10b_cfg_immediate_enable_req *req)
{
	struct ipa_es10b_eim_cfg_data *eim_cfg_data = NULL;
	int rc;

	/* The eUICC only accepts this function as long as it has no eIM configuration data (once an eIM is
	 * associated, immediate profile enabling is configured via the eIM-signed configureImmediateEnable
	 * PSMO instead, which has no such gate). */
	eim_cfg_data = ipa_es10b_get_eim_cfg_data(ctx);
	if (!eim_cfg_data) {
		rc = ConfigureImmediateProfileEnablingResponse__configImmediateEnableResult_undefinedError;
		goto error;
	}
	if (eim_cfg_data->eim_cfg_data_list_count > 0) {
		rc = ConfigureImmediateProfileEnablingResponse__configImmediateEnableResult_associatedEimAlreadyExists;
		goto error;
	}

	/* Same storage as the configureImmediateEnable PSMO emulation (es10b_load_euicc_pkg.c) */
	ctx->nvstate.iot_euicc_emu.auto_enable.flag = req->immediate_enable_flag;

	ipa_buf_free(ctx->nvstate.iot_euicc_emu.auto_enable.smdp_oid);
	ctx->nvstate.iot_euicc_emu.auto_enable.smdp_oid = NULL;
	if (req->smdp_oid)
		ctx->nvstate.iot_euicc_emu.auto_enable.smdp_oid = ipa_buf_dup(req->smdp_oid);

	ipa_buf_free(ctx->nvstate.iot_euicc_emu.auto_enable.smdp_address);
	ctx->nvstate.iot_euicc_emu.auto_enable.smdp_address = NULL;
	if (req->smdp_address)
		ctx->nvstate.iot_euicc_emu.auto_enable.smdp_address =
		    ipa_buf_alloc_data(strlen(req->smdp_address), (uint8_t *)req->smdp_address);

	rc = ConfigureImmediateProfileEnablingResponse__configImmediateEnableResult_ok;

error:
	if (rc == ConfigureImmediateProfileEnablingResponse__configImmediateEnableResult_ok) {
		IPA_LOGP_ES10X("ConfigureImmediateProfileEnabling", LERROR,
			       "IoT eUICC emulation active, function succeeded with status code %d=%s!\n", rc,
			       ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("ConfigureImmediateProfileEnabling", LERROR,
			       "IoT eUICC emulation active, function failed with error code %d=%s!\n", rc,
			       ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	}

	ipa_es10b_get_eim_cfg_data_free(eim_cfg_data);
	return rc;
}

/*! Function (ES10b): ConfigureImmediateProfileEnabling.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns positive status code on success, negative on error. */
int ipa_es10b_cfg_immediate_enable(struct ipa_context *ctx, const struct ipa_es10b_cfg_immediate_enable_req *req)
{
	if (ctx->cfg->iot_euicc_emu_enabled)
		return cfg_immediate_enable_emu(ctx, req);
	else
		return cfg_immediate_enable(ctx, req);
}
