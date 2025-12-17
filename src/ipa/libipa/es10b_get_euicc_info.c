/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, 5.7.8: Function (ES10b): GetEUICCInfo
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
#include "es10b_get_euicc_info.h"
#include <GetEuiccInfo1Request.h>
#include <GetEuiccInfo2Request.h>

static int dec_get_euicc_info1(struct ipa_es10b_euicc_info *euicc_info, const struct ipa_buf *es10b_res)
{
	struct EUICCInfo1 *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_EUICCInfo1, es10b_res, "GetEuiccInfo1Request");
	if (!asn)
		return -EINVAL;

	euicc_info->euicc_info_1 = asn;
	return 0;
}

static struct ipa_es10b_euicc_info *get_euicc_info1(struct ipa_context *ctx)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ipa_es10b_euicc_info *euicc_info = IPA_ALLOC_ZERO(struct ipa_es10b_euicc_info);
	struct GetEuiccInfo1Request get_euicc_info1_req = { 0 };
	int rc;

	/* Request minimal set of the eUICC information */
	es10b_req = ipa_es10x_req_enc(&asn_DEF_GetEuiccInfo1Request, &get_euicc_info1_req, "GetEuiccInfo1Request");
	if (!es10b_req) {
		IPA_LOGP_ES10X("GetEuiccInfo1Request", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("GetEuiccInfo1Request", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_get_euicc_info1(euicc_info, es10b_res);

	if (rc < 0)
		goto error;

	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return euicc_info;
error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	ipa_es10b_get_euicc_info_free(euicc_info);
	return NULL;
}

static void convert_euicc_info_2(struct SGP32_EUICCInfo2 *euicc_info_out, const struct EUICCInfo2 *euicc_info_in)
{
	memset(euicc_info_out, 0, sizeof(*euicc_info_out));

	euicc_info_out->profileVersion = euicc_info_in->profileVersion;
	euicc_info_out->svn = euicc_info_in->svn;
	euicc_info_out->euiccFirmwareVer = euicc_info_in->euiccFirmwareVer;
	euicc_info_out->extCardResource = euicc_info_in->extCardResource;
	euicc_info_out->uiccCapability = euicc_info_in->uiccCapability;
	euicc_info_out->ts102241Version = euicc_info_in->ts102241Version;
	euicc_info_out->globalplatformVersion = euicc_info_in->globalplatformVersion;
	euicc_info_out->rspCapability = euicc_info_in->rspCapability;
	euicc_info_out->euiccCiPKIdListForVerification.list.count =
	    euicc_info_in->euiccCiPKIdListForVerification.list.count;
	euicc_info_out->euiccCiPKIdListForVerification.list.array =
	    euicc_info_in->euiccCiPKIdListForVerification.list.array;
	euicc_info_out->euiccCiPKIdListForSigning.list.count = euicc_info_in->euiccCiPKIdListForSigning.list.count;
	euicc_info_out->euiccCiPKIdListForSigning.list.array = euicc_info_in->euiccCiPKIdListForSigning.list.array;
	euicc_info_out->euiccCategory = euicc_info_in->euiccCategory;
	euicc_info_out->forbiddenProfilePolicyRules = euicc_info_in->forbiddenProfilePolicyRules;
	euicc_info_out->ppVersion = euicc_info_in->ppVersion;
	euicc_info_out->sasAcreditationNumber = euicc_info_in->sasAcreditationNumber;
	euicc_info_out->certificationDataObject = euicc_info_in->certificationDataObject;
}

static int dec_get_euicc_info2(struct ipa_es10b_euicc_info *euicc_info, const struct ipa_buf *es10b_res)
{
	struct EUICCInfo2 *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_EUICCInfo2, es10b_res, "GetEuiccInfo2Request");
	if (!asn)
		return -EINVAL;

	euicc_info->euicc_info_2 = asn;

	/* Also offer EUICCInfo2 in SGP.32 format */
	euicc_info->sgp32_euicc_info_2 = IPA_ALLOC(struct SGP32_EUICCInfo2);
	convert_euicc_info_2(euicc_info->sgp32_euicc_info_2, euicc_info->euicc_info_2);

	return 0;
}

static int dec_get_euicc_info2_sgp32(struct ipa_es10b_euicc_info *euicc_info, const struct ipa_buf *es10b_res)
{
	struct SGP32_EUICCInfo2 *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_SGP32_EUICCInfo2, es10b_res, "GetEuiccInfo2Request");
	if (!asn)
		return -EINVAL;

	euicc_info->sgp32_euicc_info_2 = asn;

	return 0;
}

static struct ipa_es10b_euicc_info *get_euicc_info2(struct ipa_context *ctx)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ipa_es10b_euicc_info *euicc_info = IPA_ALLOC_ZERO(struct ipa_es10b_euicc_info);
	struct GetEuiccInfo1Request get_euicc_info2_req = { 0 };
	int rc;

	/* Request full set of the eUICC information */
	es10b_req = ipa_es10x_req_enc(&asn_DEF_GetEuiccInfo2Request, &get_euicc_info2_req, "GetEuiccInfo2Request");
	if (!es10b_req) {
		IPA_LOGP_ES10X("GetEuiccInfo2Request", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("GetEuiccInfo2Request", LERROR, "no ES10b response\n");
		goto error;
	}

	if (ctx->cfg->iot_euicc_emu_enabled) {
		IPA_LOGP_ES10X("GetEuiccInfo2Request", LINFO, "IoT eUICC emulation active, will derive SGP32-EUICCInfo2 from (SGP.22) EUICCInfo2.\n");
		rc = dec_get_euicc_info2(euicc_info, es10b_res);
	} else {
		rc = dec_get_euicc_info2_sgp32(euicc_info, es10b_res);
	}
	if (rc < 0)
		goto error;


	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return euicc_info;
error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	ipa_es10b_get_euicc_info_free(euicc_info);
	return NULL;
}

/*! Function (ES10b): GetEUICCInfo.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] full set to true to request EUICCInfo2 instead of EUICCInfo1.
 *  \returns struct with parsed eUICC info on success, NULL on failure. */
struct ipa_es10b_euicc_info *ipa_es10b_get_euicc_info(struct ipa_context *ctx, bool full)
{
	if (full)
		return get_euicc_info2(ctx);
	else
		return get_euicc_info1(ctx);
}

/*! Free results of function (ES10b): GetEUICCInfo.
 *  \param[in] res pointer to function result. */
void ipa_es10b_get_euicc_info_free(struct ipa_es10b_euicc_info *res)
{
	if (!res)
		return;

	ASN_STRUCT_FREE(asn_DEF_EUICCInfo1, res->euicc_info_1);

	if (res->euicc_info_2) {
		IPA_FREE(res->sgp32_euicc_info_2);
		ASN_STRUCT_FREE(asn_DEF_EUICCInfo2, res->euicc_info_2);
	} else {
		ASN_STRUCT_FREE(asn_DEF_SGP32_EUICCInfo2, res->sgp32_euicc_info_2);
	}

	IPA_FREE(res);
}
