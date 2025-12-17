/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, 5.7.19: Function (ES10b): eUICCMemoryReset
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <EuiccMemoryResetRequest.h>
#include <EuiccMemoryResetResponse.h>
#include <SGP32-EuiccMemoryResetRequest.h>
#include <SGP32-EuiccMemoryResetResponse.h>
#include "context.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_euicc_mem_rst.h"

static const struct num_str_map error_code_strings_resetResult[] = {
	{ EuiccMemoryResetResponse__resetResult_ok, "ok" },
	{ EuiccMemoryResetResponse__resetResult_nothingToDelete, "nothingToDelete" },
	{ EuiccMemoryResetResponse__resetResult_catBusy, "catBusy" },
	{ EuiccMemoryResetResponse__resetResult_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static const struct num_str_map sgp32_error_code_strings_resetResult[] = {
	{ SGP32_EuiccMemoryResetResponse__resetResult_ok, "ok" },
	{ SGP32_EuiccMemoryResetResponse__resetResult_nothingToDelete, "nothingToDelete" },
	{ SGP32_EuiccMemoryResetResponse__resetResult_catBusy, "catBusy" },
	{ SGP32_EuiccMemoryResetResponse__resetResult_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static const struct num_str_map sgp32_error_code_strings_resetEimResult[] = {
	{ SGP32_EuiccMemoryResetResponse__resetEimResult_ok, "ok" },
	{ SGP32_EuiccMemoryResetResponse__resetEimResult_nothingToDelete, "nothingToDelete" },
	{ SGP32_EuiccMemoryResetResponse__resetEimResult_eimResetNotSupported, "eimResetNotSupported" },
	{ SGP32_EuiccMemoryResetResponse__resetEimResult_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static const struct num_str_map sgp32_error_code_strings_resetAutoEnableConfigResult[] = {
	{ SGP32_EuiccMemoryResetResponse__resetAutoEnableConfigResult_ok, "ok" },
	{ SGP32_EuiccMemoryResetResponse__resetAutoEnableConfigResult_resetAECNotSupported, "nothingToDelete" },
	{ SGP32_EuiccMemoryResetResponse__resetAutoEnableConfigResult_undefinedError, "eimResetNotSupported" },
	{ 0, NULL }
};

static int dec_euicc_mem_rst_res_sgp32(const struct ipa_buf *es10b_res)
{
	struct SGP32_EuiccMemoryResetResponse *asn = NULL;
	int rc;

	asn = ipa_es10x_res_dec(&asn_DEF_SGP32_EuiccMemoryResetResponse, es10b_res, "eUICCMemoryReset");
	if (!asn)
		return -EINVAL;

	if (asn->resetResult != SGP32_EuiccMemoryResetResponse__resetResult_ok &&
	    asn->resetResult != SGP32_EuiccMemoryResetResponse__resetResult_nothingToDelete) {
		IPA_LOGP_ES10X("eUICCMemoryReset", LERROR, "function failed with error code %ld=%s!\n",
			       asn->resetResult, ipa_str_from_num(sgp32_error_code_strings_resetResult,
								  asn->resetResult, "(unknown)"));
		rc = -EINVAL;
	} else {
		IPA_LOGP_ES10X("eUICCMemoryReset", LERROR, "function succeeded with status code %ld=%s!\n",
			       asn->resetResult, ipa_str_from_num(sgp32_error_code_strings_resetResult,
								  asn->resetResult, "(unknown)"));
	}

	if (asn->resetResult != SGP32_EuiccMemoryResetResponse__resetEimResult_ok &&
	    asn->resetResult != SGP32_EuiccMemoryResetResponse__resetEimResult_nothingToDelete) {
		IPA_LOGP_ES10X("eUICCMemoryReset", LERROR, "function failed with error code %ld=%s!\n",
			       asn->resetResult, ipa_str_from_num(sgp32_error_code_strings_resetEimResult,
								  asn->resetResult, "(unknown)"));
		rc = -EINVAL;
	} else {
		IPA_LOGP_ES10X("eUICCMemoryReset", LERROR, "function succeeded with status code %ld=%s!\n",
			       asn->resetResult, ipa_str_from_num(sgp32_error_code_strings_resetEimResult,
								  asn->resetResult, "(unknown)"));
	}

	if (asn->resetResult != SGP32_EuiccMemoryResetResponse__resetAutoEnableConfigResult_ok) {
		IPA_LOGP_ES10X("eUICCMemoryReset", LERROR, "function failed with error code %ld=%s!\n",
			       asn->resetResult, ipa_str_from_num(sgp32_error_code_strings_resetAutoEnableConfigResult,
								  asn->resetResult, "(unknown)"));
		rc = -EINVAL;
	} else {
		IPA_LOGP_ES10X("eUICCMemoryReset", LERROR, "function succeeded with status code %ld=%s!\n",
			       asn->resetResult, ipa_str_from_num(sgp32_error_code_strings_resetAutoEnableConfigResult,
								  asn->resetResult, "(unknown)"));
	}

	ASN_STRUCT_FREE(asn_DEF_EuiccMemoryResetResponse, asn);
	return rc;
}

static int dec_euicc_mem_rst_res(const struct ipa_buf *es10b_res)
{
	struct EuiccMemoryResetResponse *asn = NULL;
	int rc;

	asn = ipa_es10x_res_dec(&asn_DEF_EuiccMemoryResetResponse, es10b_res, "eUICCMemoryReset");
	if (!asn)
		return -EINVAL;

	if (asn->resetResult != EuiccMemoryResetResponse__resetResult_ok &&
	    asn->resetResult != EuiccMemoryResetResponse__resetResult_nothingToDelete) {
		IPA_LOGP_ES10X("eUICCMemoryReset", LERROR, "function failed with error code %ld=%s!\n",
			       asn->resetResult, ipa_str_from_num(error_code_strings_resetResult, asn->resetResult,
								  "(unknown)"));
		rc = -EINVAL;
	} else {
		IPA_LOGP_ES10X("eUICCMemoryReset", LERROR, "function succeeded with status code %ld=%s!\n",
			       asn->resetResult, ipa_str_from_num(error_code_strings_resetResult, asn->resetResult,
								  "(unknown)"));
	}

	ASN_STRUCT_FREE(asn_DEF_EuiccMemoryResetResponse, asn);
	return rc;
}

int euicc_mem_rst(struct ipa_context *ctx, const struct ipa_es10b_euicc_mem_rst *req)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct SGP32_EuiccMemoryResetRequest mem_rst_req = { 0 };
	int rc = -EINVAL;
	uint8_t rst_opt[1] = { 0 };

	mem_rst_req.resetOptions.buf = rst_opt;
	mem_rst_req.resetOptions.size = 1;
	mem_rst_req.resetOptions.bits_unused = 3;

	if (req->operatnl_profiles)
		rst_opt[0] |= (1 << (7 - SGP32_EuiccMemoryResetRequest__resetOptions_deleteOperationalProfiles));
	if (req->test_profiles)
		rst_opt[0] |= (1 << (7 - SGP32_EuiccMemoryResetRequest__resetOptions_deleteFieldLoadedTestProfiles));
	if (req->default_smdp_addr)
		rst_opt[0] |= (1 << (7 - SGP32_EuiccMemoryResetRequest__resetOptions_resetDefaultSmdpAddress));
	if (req->eim_cfg_data)
		rst_opt[0] |= (1 << (7 - SGP32_EuiccMemoryResetRequest__resetOptions_resetEimConfigData));
	if (req->auto_enable_cfg)
		rst_opt[0] |= (1 << (7 - SGP32_EuiccMemoryResetRequest__resetOptions_resetAutoEnableConfig));

	es10b_req = ipa_es10x_req_enc(&asn_DEF_SGP32_EuiccMemoryResetRequest, &mem_rst_req, "eUICCMemoryReset");
	if (!es10b_req) {
		IPA_LOGP_ES10X("eUICCMemoryReset", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("eUICCMemoryReset", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_euicc_mem_rst_res_sgp32(es10b_res);
	if (rc < 0)
		goto error;

error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return rc;
}

int euicc_mem_rst_emu(struct ipa_context *ctx, const struct ipa_es10b_euicc_mem_rst *req)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct EuiccMemoryResetRequest mem_rst_req = { 0 };
	int rc = -EINVAL;
	uint8_t rst_opt[1] = { 0 };

	mem_rst_req.resetOptions.buf = rst_opt;
	mem_rst_req.resetOptions.size = 1;
	mem_rst_req.resetOptions.bits_unused = 6;

	if (req->operatnl_profiles)
		rst_opt[0] |= (1 << (7 - EuiccMemoryResetRequest__resetOptions_deleteOperationalProfiles));
	if (req->test_profiles)
		rst_opt[0] |= (1 << (7 - EuiccMemoryResetRequest__resetOptions_deleteFieldLoadedTestProfiles));
	if (req->default_smdp_addr)
		rst_opt[0] |= (1 << (7 - EuiccMemoryResetRequest__resetOptions_resetDefaultSmdpAddress));

	if (req->eim_cfg_data) {
		IPA_LOGP_ES10X("eUICCMemoryReset", LINFO,
			       "IoT eUICC emulation active, also clearing memory with eIM configuration...\n");
		IPA_FREE(ctx->nvstate.iot_euicc_emu.eim_cfg_ber);
		ctx->nvstate.iot_euicc_emu.eim_cfg_ber = NULL;
	}
	if (req->auto_enable_cfg) {
		IPA_LOGP_ES10X("eUICCMemoryReset", LINFO,
			       "IoT eUICC emulation active, also clearing auto enable configuration...\n");
		IPA_FREE(ctx->nvstate.iot_euicc_emu.auto_enable.smdp_oid);
		ctx->nvstate.iot_euicc_emu.auto_enable.smdp_oid = NULL;
		IPA_FREE(ctx->nvstate.iot_euicc_emu.auto_enable.smdp_address);
		ctx->nvstate.iot_euicc_emu.auto_enable.smdp_address = NULL;
		ctx->nvstate.iot_euicc_emu.auto_enable.flag = false;
	}

	es10b_req = ipa_es10x_req_enc(&asn_DEF_EuiccMemoryResetRequest, &mem_rst_req, "eUICCMemoryReset");
	if (!es10b_req) {
		IPA_LOGP_ES10X("eUICCMemoryReset", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("eUICCMemoryReset", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_euicc_mem_rst_res(es10b_res);
	if (rc < 0)
		goto error;

error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return rc;
}

/*! Function (ES10b): eUICCMemoryReset.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns 0 on success, negative on error. */
int ipa_es10b_euicc_mem_rst(struct ipa_context *ctx, const struct ipa_es10b_euicc_mem_rst *req)
{
	if (ctx->cfg->iot_euicc_emu_enabled)
		return euicc_mem_rst_emu(ctx, req);
	else
		return euicc_mem_rst(ctx, req);
}
