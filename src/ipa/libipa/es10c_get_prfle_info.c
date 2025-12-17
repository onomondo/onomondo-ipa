/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, 5.7.15: Function (ES10c): GetProfilesInfo
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
#include "es10c_get_prfle_info.h"

static const struct num_str_map error_code_strings[] = {
	{ ProfileInfoListError_incorrectInputValues, "incorrectInputValues" },
	{ ProfileInfoListError_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static const struct num_str_map error_code_strings_sgp32[] = {
	{ SGP32_ProfileInfoListError_incorrectInputValues, "incorrectInputValues" },
	{ SGP32_ProfileInfoListError_profileChangeOngoing, "profileChangeOngoing" },
	{ SGP32_ProfileInfoListError_undefinedError, "undefinedError" },
	{ 0, NULL }
};

/* Convert the response into SGP32 format */
static struct SGP32_ProfileInfoListResponse *convert_res_to_sgp32(struct ProfileInfoListResponse *res)
{
	struct SGP32_ProfileInfoListResponse *sgp32_res;
	struct ProfileInfo *prfle_info_item;
	unsigned int i;

	if (!res)
		return NULL;

	sgp32_res = IPA_ALLOC_ZERO(struct SGP32_ProfileInfoListResponse);
	assert(sgp32_res);

	switch (res->present) {
	case ProfileInfoListResponse_PR_profileInfoListOk:
		for (i = 0; i < res->choice.profileInfoListOk.list.count; i++) {
			prfle_info_item = IPA_ALLOC(struct ProfileInfo);
			*prfle_info_item = *res->choice.profileInfoListOk.list.array[i];
			ASN_SEQUENCE_ADD(&sgp32_res->choice.profileInfoListOk.list, prfle_info_item);
		}

		sgp32_res->present = SGP32_ProfileInfoListResponse_PR_profileInfoListOk;
		break;
	case ProfileInfoListResponse_PR_profileInfoListError:
		switch (res->choice.profileInfoListError) {
		case ProfileInfoListError_incorrectInputValues:
			sgp32_res->choice.profileInfoListError = SGP32_ProfileInfoListError_incorrectInputValues;
			break;
		default:
			sgp32_res->choice.profileInfoListError = ProfileInfoListError_undefinedError;
		}
		break;
		sgp32_res->present = SGP32_ProfileInfoListResponse_PR_profileInfoListError;
	default:
		sgp32_res->present = SGP32_ProfileInfoListResponse_PR_NOTHING;
	}

	return sgp32_res;
}

static int dec_get_prfle_info_res(struct ipa_es10c_get_prfle_info_res *res, const struct ipa_buf *es10c_res)
{
	struct ProfileInfoListResponse *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_ProfileInfoListResponse, es10c_res, "GetProfilesInfo");
	if (!asn)
		return -EINVAL;

	switch (asn->present) {
	case ProfileInfoListResponse_PR_profileInfoListError:
		res->prfle_info_list_err = asn->choice.profileInfoListError;
		IPA_LOGP_ES10X("GetProfilesInfo", LERROR, "function failed with error code %ld=%s!\n",
			       res->prfle_info_list_err, ipa_str_from_num(error_code_strings, res->prfle_info_list_err,
									  "(unknown)"));
		break;
	case ProfileInfoListResponse_PR_profileInfoListOk:
		/* Nothing to do */
		break;
	default:
		IPA_LOGP_ES10X("GetProfilesInfo", LERROR, "unexpected response content!\n");
		res->prfle_info_list_err = -1;
	}

	res->sgp32_res = convert_res_to_sgp32(asn);
	res->res = asn;
	return 0;
}

static int dec_get_prfle_info_res_sgp32(struct ipa_es10c_get_prfle_info_res *res, const struct ipa_buf *es10c_res)
{
	struct SGP32_ProfileInfoListResponse *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_SGP32_ProfileInfoListResponse, es10c_res, "GetProfilesInfo");
	if (!asn)
		return -EINVAL;

	switch (asn->present) {
	case SGP32_ProfileInfoListResponse_PR_profileInfoListError:
		res->prfle_info_list_err = asn->choice.profileInfoListError;
		IPA_LOGP_ES10X("GetProfilesInfo", LERROR, "function failed with error code %ld=%s!\n",
			       res->prfle_info_list_err, ipa_str_from_num(error_code_strings_sgp32,
									  res->prfle_info_list_err, "(unknown)"));
		break;
	case SGP32_ProfileInfoListResponse_PR_profileInfoListOk:
		/* Nothing to do */
		break;
	default:
		IPA_LOGP_ES10X("GetProfilesInfo", LERROR, "unexpected response content!\n");
		res->prfle_info_list_err = -1;
	}

	res->sgp32_res = asn;
	return 0;
}

/* Find the currently active profile */
static void find_currently_active_prfle(struct ipa_es10c_get_prfle_info_res *res)
{
	unsigned int i;
	struct ProfileInfo *prfle_info;
	res->currently_active_prfle = NULL;

	if (res->sgp32_res->present != SGP32_ProfileInfoListResponse_PR_profileInfoListOk)
		return;

	for (i = 0; i < res->sgp32_res->choice.profileInfoListOk.list.count; i++) {
		prfle_info = res->sgp32_res->choice.profileInfoListOk.list.array[i];
		if (!prfle_info->profileState)
			continue;
		if (*prfle_info->profileState == ProfileState_enabled) {
			res->currently_active_prfle = res->sgp32_res->choice.profileInfoListOk.list.array[i];
			return;
		}
	}
}

/*! Function (Es10c): GetProfilesInfo.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters (may be NULL to request full info).
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10c_get_prfle_info_res *ipa_es10c_get_prfle_info(struct ipa_context *ctx,
							      const struct ipa_es10c_get_prfle_info_req *req)
{
	struct ipa_buf *es10c_req = NULL;
	struct ipa_buf *es10c_res = NULL;
	struct ipa_es10c_get_prfle_info_res *res = IPA_ALLOC_ZERO(struct ipa_es10c_get_prfle_info_res);
	int rc;
	const struct ipa_es10c_get_prfle_info_req req_all = { 0 };

	if (!req)
		req = &req_all;

	es10c_req = ipa_es10x_req_enc(&asn_DEF_ProfileInfoListRequest, &req->req, "GetProfilesInfo");
	if (!es10c_req) {
		IPA_LOGP_ES10X("GetProfilesInfo", LERROR, "unable to encode Es10c request\n");
		goto error;
	}

	es10c_res = ipa_euicc_transceive_es10x(ctx, es10c_req);
	if (!es10c_res) {
		IPA_LOGP_ES10X("GetProfilesInfo", LERROR, "no Es10c response\n");
		goto error;
	}

	if (ctx->cfg->iot_euicc_emu_enabled) {
		IPA_LOGP_ES10X("GetProfilesInfo", LINFO,
			       "IoT eUICC emulation active, will derive SGP32_ProfileInfoListResponse from (SGP.22) ProfileInfoListResponse.\n");
		rc = dec_get_prfle_info_res(res, es10c_res);
	} else {
		rc = dec_get_prfle_info_res_sgp32(res, es10c_res);
	}
	if (rc < 0)
		goto error;

	find_currently_active_prfle(res);

	IPA_FREE(es10c_req);
	IPA_FREE(es10c_res);
	return res;
error:
	IPA_FREE(es10c_req);
	IPA_FREE(es10c_res);
	ipa_es10c_get_prfle_info_res_free(res);
	return NULL;
}

/*! Free results of function (Es10c): GetProfilesInfo.
 *  \param[in] res pointer to function result. */
void ipa_es10c_get_prfle_info_res_free(struct ipa_es10c_get_prfle_info_res *res)
{
	unsigned int i;

	if (!res)
		return;

	if (res->res) {
		if (res->sgp32_res) {
			if (res->sgp32_res->present == SGP32_ProfileInfoListResponse_PR_profileInfoListOk) {
				for (i = 0; i < res->sgp32_res->choice.profileInfoListOk.list.count; i++)
					IPA_FREE(res->sgp32_res->choice.profileInfoListOk.list.array[i]);
				IPA_FREE(res->sgp32_res->choice.profileInfoListOk.list.array);
			}
			IPA_FREE(res->sgp32_res);
		}
		ASN_STRUCT_FREE(asn_DEF_ProfileInfoListResponse, res->res);
	} else {
		ASN_STRUCT_FREE(asn_DEF_SGP32_ProfileInfoListResponse, res->sgp32_res);
	}

	IPA_FREE(res);
}
