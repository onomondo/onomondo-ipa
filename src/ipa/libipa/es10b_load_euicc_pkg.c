/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.32, 5.9.1: Function (ES10b): LoadEuiccPackage
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
#include "es10b_load_euicc_pkg.h"
#include "es10c_enable_prfle.h"
#include "es10c_disable_prfle.h"
#include "es10c_delete_prfle.h"
#include "es10c_get_prfle_info.h"
#include "es10b_get_eim_cfg_data.h"
#include "es10b_add_init_eim.h"
#include "es10b_get_rat.h"

static void update_rollback_iccid(struct ipa_context *ctx)
{
	struct ipa_es10c_get_prfle_info_res *get_prfle_info_res = NULL;

	get_prfle_info_res = ipa_es10c_get_prfle_info(ctx, NULL);
	if (!get_prfle_info_res || get_prfle_info_res->prfle_info_list_err != 0) {
		IPA_LOGP(SIPA, LERROR, "error reading profile info!\n");
		return;
	}

	if (get_prfle_info_res->res && get_prfle_info_res->currently_active_prfle) {
		if (!get_prfle_info_res->currently_active_prfle->iccid) {
			IPA_LOGP(SIPA, LERROR,
				 "a profile is active, but it does not have an ICCID, cannot use this profile for rollback!\n");
			return;
		}
		IPA_FREE(ctx->iot_euicc_emu.rollback_iccid);
		ctx->iot_euicc_emu.rollback_iccid = IPA_BUF_FROM_ASN(get_prfle_info_res->currently_active_prfle->iccid);
		IPA_LOGP(SIPA, LINFO, "will use ICCD:%s in case of profile rollback.\n",
			 ipa_buf_hexdump(ctx->iot_euicc_emu.rollback_iccid));
	} else {
		IPA_FREE(ctx->iot_euicc_emu.rollback_iccid);
		ctx->iot_euicc_emu.rollback_iccid = NULL;
		IPA_LOGP(SIPA, LINFO, "no profile active, profile rollback not possible.\n");
	}

	ipa_es10c_get_prfle_info_res_free(get_prfle_info_res);
}

static int dec_load_euicc_pkg_res(struct ipa_es10b_load_euicc_pkg_res *res, const struct ipa_buf *es10b_res)
{
	struct EuiccPackageResult *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_EuiccPackageResult, es10b_res, "LoadEuiccPackage");
	if (!asn)
		return -EINVAL;

	res->res = asn;
	return 0;
}

struct ipa_es10b_load_euicc_pkg_res *load_euicc_pkg(struct ipa_context *ctx,
						    const struct ipa_es10b_load_euicc_pkg_req *req)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ipa_es10b_load_euicc_pkg_res *res = IPA_ALLOC_ZERO(struct ipa_es10b_load_euicc_pkg_res);
	int rc;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_EuiccPackageRequest, &req->req, "LoadEuiccPackage");
	if (!es10b_req) {
		IPA_LOGP_ES10X("LoadEuiccPackage", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("LoadEuiccPackage", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_load_euicc_pkg_res(res, es10b_res);
	if (rc < 0)
		goto error;

	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return res;
error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	ipa_es10b_load_euicc_pkg_res_free(res);
	return NULL;
}

struct EuiccResultData *iot_emo_do_enable_psmo(struct ipa_context *ctx, const struct Psmo__enable *enable_psmo)
{
	struct EuiccResultData *euicc_result_data = IPA_ALLOC_ZERO(struct EuiccResultData);
	struct ipa_es10c_enable_prfle_req enable_prfle_req = { 0 };
	struct ipa_es10c_enable_prfle_res *enable_prfle_res = NULL;

	euicc_result_data->present = EuiccResultData_PR_enableResult;

	/* With the IoT/PSMO interface we can only identify the profile via its ICCID */
	enable_prfle_req.req.profileIdentifier.present = EnableProfileRequest__profileIdentifier_PR_iccid;
	enable_prfle_req.req.profileIdentifier.choice.iccid = enable_psmo->iccid;

	/* There is no way to tell the eUICC via the IoT/PSMO interface when a refresh is required. */
	enable_prfle_req.req.refreshFlag = ctx->cfg->refresh_flag;
	enable_prfle_res = ipa_es10c_enable_prfle(ctx, &enable_prfle_req);
	if (enable_prfle_res)
		euicc_result_data->choice.enableResult = enable_prfle_res->res->enableResult;
	else
		euicc_result_data->choice.enableResult = EnableProfileResult_undefinedError;

	ipa_es10c_enable_prfle_res_free(enable_prfle_res);
	return euicc_result_data;
}

struct EuiccResultData *iot_emo_do_disable_psmo(struct ipa_context *ctx, const struct Psmo__disable *disable_psmo)
{
	struct EuiccResultData *euicc_result_data = IPA_ALLOC_ZERO(struct EuiccResultData);
	struct ipa_es10c_disable_prfle_req disable_prfle_req = { 0 };
	struct ipa_es10c_disable_prfle_res *disable_prfle_res = NULL;

	euicc_result_data->present = EuiccResultData_PR_disableResult;

	/* With the IoT/PSMO interface we can only identify the profile via its ICCID */
	disable_prfle_req.req.profileIdentifier.present = DisableProfileRequest__profileIdentifier_PR_iccid;
	disable_prfle_req.req.profileIdentifier.choice.iccid = disable_psmo->iccid;

	/* There is no way to tell the eUICC via the IoT/PSMO interface when a refresh is required. */
	disable_prfle_req.req.refreshFlag = ctx->cfg->refresh_flag;
	disable_prfle_res = ipa_es10c_disable_prfle(ctx, &disable_prfle_req);
	if (disable_prfle_res)
		euicc_result_data->choice.disableResult = disable_prfle_res->res->disableResult;
	else
		euicc_result_data->choice.disableResult = DisableProfileResult_undefinedError;

	ipa_es10c_disable_prfle_res_free(disable_prfle_res);
	return euicc_result_data;
}

struct EuiccResultData *iot_emo_do_delete_psmo(struct ipa_context *ctx, const struct Psmo__delete *delete_psmo)
{
	struct EuiccResultData *euicc_result_data = IPA_ALLOC_ZERO(struct EuiccResultData);
	struct ipa_es10c_delete_prfle_req delete_prfle_req = { 0 };
	struct ipa_es10c_delete_prfle_res *delete_prfle_res = NULL;

	euicc_result_data->present = EuiccResultData_PR_deleteResult;

	/* With the IoT/PSMO interface we can only identify the profile via its ICCID */
	delete_prfle_req.req.present = DeleteProfileRequest_PR_iccid;
	delete_prfle_req.req.choice.iccid = delete_psmo->iccid;

	delete_prfle_res = ipa_es10c_delete_prfle(ctx, &delete_prfle_req);
	if (delete_prfle_res)
		euicc_result_data->choice.deleteResult = delete_prfle_res->res->deleteResult;
	else
		euicc_result_data->choice.deleteResult = DeleteProfileResult_undefinedError;

	ipa_es10c_delete_prfle_res_free(delete_prfle_res);
	return euicc_result_data;
}

struct EuiccResultData *iot_emo_do_listProfileInfo_psmo(struct ipa_context *ctx,
							const struct ProfileInfoListRequest *listProfileInfo_psmo)
{
	struct EuiccResultData *euicc_result_data = IPA_ALLOC_ZERO(struct EuiccResultData);
	struct ipa_es10c_get_prfle_info_req get_prfle_info_req = { 0 };
	struct ipa_es10c_get_prfle_info_res *get_prfle_info_res = NULL;
	struct SGP32_ProfileInfoListResponse *prfle_info_res;

	euicc_result_data->present = EuiccResultData_PR_listProfileInfoResult;

	get_prfle_info_req.req = *listProfileInfo_psmo;
	get_prfle_info_res = ipa_es10c_get_prfle_info(ctx, &get_prfle_info_req);
	if (!get_prfle_info_res) {
		prfle_info_res = IPA_ALLOC_ZERO(struct SGP32_ProfileInfoListResponse);
		assert(prfle_info_res);
		prfle_info_res->present = SGP32_ProfileInfoListResponse_PR_profileInfoListError;
		prfle_info_res->choice.profileInfoListError = SGP32_ProfileInfoListError_undefinedError;
		euicc_result_data->choice.listProfileInfoResult = *prfle_info_res;
	} else {
		/* Place a full copy of the contents of get_prfle_info_res->sgp32_res into
		 * euicc_result_data->choice.listProfileInfoResult. This is necessary because we want to free
		 * get_prfle_info_res on return */
		prfle_info_res = ipa_asn1c_dup(&asn_DEF_SGP32_ProfileInfoListResponse, get_prfle_info_res->sgp32_res);
		assert(prfle_info_res);
		euicc_result_data->choice.listProfileInfoResult = *prfle_info_res;
		IPA_FREE(prfle_info_res);	/* free outer shell only */
	}

	ipa_es10c_get_prfle_info_res_free(get_prfle_info_res);
	return euicc_result_data;
}

struct EuiccResultData *iot_emo_do_getRAT_psmo(struct ipa_context *ctx, const struct Psmo__getRAT *getRAT_psmo)
{
	struct EuiccResultData *euicc_result_data = IPA_ALLOC_ZERO(struct EuiccResultData);
	struct ipa_es10b_get_rat_res *get_rat_res = NULL;
	struct ProfilePolicyAuthorisationRule *ppr_item;
	unsigned int i;

	euicc_result_data->present = EuiccResultData_PR_getRATResult;

	get_rat_res = ipa_es10b_get_rat(ctx);
	if (!get_rat_res || !get_rat_res->res) {
		/* The protocol does not allow to communicate an error back to the eIM. Errors are communicated
		 * implicitly by sending back an empty RAT. */
		IPA_LOGP_ES10X("LoadEuiccPackage", LERROR,
			       "IoT eUICC emulation active, getRAT PSMO failed, unable to retrieve RulesAuthorisationTable!\n");
	} else {
		for (i = 0; i < get_rat_res->res->rat.list.count; i++) {
			ppr_item =
			    ipa_asn1c_dup(&asn_DEF_ProfilePolicyAuthorisationRule, get_rat_res->res->rat.list.array[i]);
			ASN_SEQUENCE_ADD(&euicc_result_data->choice.getRATResult.list, ppr_item);
		}
	}

	ipa_es10b_get_rat_res_free(get_rat_res);
	return euicc_result_data;
}

struct EuiccResultData *iot_emo_do_configureAutoEnable_psmo(struct ipa_context *ctx, const struct Psmo__configureAutoEnable
							    *configureAutoEnable_psmo)
{
	struct EuiccResultData *euicc_result_data = IPA_ALLOC_ZERO(struct EuiccResultData);

	euicc_result_data->present = EuiccResultData_PR_configureAutoEnableResult;

	/* Update autoEnableFlag */
	if (configureAutoEnable_psmo->autoEnableFlag)
		ctx->nvstate.iot_euicc_emu.auto_enable.flag = true;
	else
		ctx->nvstate.iot_euicc_emu.auto_enable.flag = false;

	/* Update smdpOid */
	ipa_buf_free(ctx->nvstate.iot_euicc_emu.auto_enable.smdp_oid);
	ctx->nvstate.iot_euicc_emu.auto_enable.smdp_oid = NULL;
	if (configureAutoEnable_psmo->smdpOid)
		ctx->nvstate.iot_euicc_emu.auto_enable.smdp_oid = IPA_BUF_FROM_ASN(configureAutoEnable_psmo->smdpOid);

	/* Update smdpAddress */
	ipa_buf_free(ctx->nvstate.iot_euicc_emu.auto_enable.smdp_address);
	ctx->nvstate.iot_euicc_emu.auto_enable.smdp_address = NULL;
	if (configureAutoEnable_psmo->smdpAddress)
		ctx->nvstate.iot_euicc_emu.auto_enable.smdp_address =
		    IPA_BUF_FROM_ASN(configureAutoEnable_psmo->smdpAddress);

	euicc_result_data->choice.configureAutoEnableResult = ConfigureAutoEnableResult_ok;

	return euicc_result_data;
}

struct EuiccResultData *iot_emo_do_addEim_eco(struct ipa_context *ctx, const struct EimConfigurationData *addEim_eco)
{
	struct EuiccResultData *euicc_result_data = IPA_ALLOC_ZERO(struct EuiccResultData);
	struct ipa_es10b_eim_cfg_data *eim_cfg_data = NULL;
	struct ipa_es10b_add_init_eim_req add_init_eim_req = { 0 };
	struct ipa_es10b_add_init_eim_res *add_init_eim_res = NULL;
	unsigned int i;
	struct EimConfigurationData *eim_cfg_data_item;

	euicc_result_data->present = EuiccResultData_PR_addEimResult;
	euicc_result_data->choice.addEimResult.present = AddEimResult_PR_addEimResultCode;
	euicc_result_data->choice.addEimResult.choice.addEimResultCode = AddEimResult__addEimResultCode_undefinedError;

	/* Decode existing eIM configuration */
	eim_cfg_data = ipa_es10b_get_eim_cfg_data(ctx);
	if (!eim_cfg_data || !eim_cfg_data->res) {
		IPA_LOGP_ES10X("LoadEuiccPackage", LERROR,
			       "IoT eUICC emulation active, addEim eCO failed, unable to retrieve eimConfigurationData!\n");
		goto error;
	}

	/* First: copy all existing eimConfiguration entries */
	for (i = 0; i < eim_cfg_data->res->eimConfigurationDataList.list.count; i++) {
		eim_cfg_data_item = eim_cfg_data->res->eimConfigurationDataList.list.array[i];
		ASN_SEQUENCE_ADD(&add_init_eim_req.req.eimConfigurationDataList.list, eim_cfg_data_item);
		if (IPA_ASN_STR_CMP(&eim_cfg_data_item->eimId, &addEim_eco->eimId)) {
			IPA_LOGP_ES10X("LoadEuiccPackage", LERROR,
				       "IoT eUICC emulation active, addEim eCO failed, eIM with specified eimId already exists!\n");
			euicc_result_data->choice.addEimResult.present = AddEimResult_PR_addEimResultCode;
			euicc_result_data->choice.addEimResult.choice.addEimResultCode =
			    AddEimResult__addEimResultCode_commandError;
			goto error;
		}
	}

	/* Second: copy the eimConfiguration entry we want to add */
	eim_cfg_data_item = (struct EimConfigurationData *)addEim_eco;
	ASN_SEQUENCE_ADD(&add_init_eim_req.req.eimConfigurationDataList.list, eim_cfg_data_item);

	/* Write new eIM configuration by executing ES10b:AddInitialEim. This will work since the IoT eUICC emulation
	 * does not check if there is already an eIM configuration in place. It will just overwrite the existing
	 * configuration with the one we have just assembled above. */
	add_init_eim_res = ipa_es10b_add_init_eim(ctx, &add_init_eim_req);
	if (!add_init_eim_res || add_init_eim_res->add_init_eim_err || !add_init_eim_res->res
	    || add_init_eim_res->res->present != AddInitialEimResponse_PR_addInitialEimOk) {
		IPA_LOGP_ES10X("LoadEuiccPackage", LERROR,
			       "IoT eUICC emulation active, addEim eCO failed, unable to write eimConfigurationData!\n");
		goto error;
	}

	/* Look at the last item of the response list, there we will find the result for the eIM configuration
	 * that we have just added. */
	switch (add_init_eim_res->res->choice.addInitialEimOk.
		list.array[add_init_eim_res->res->choice.addInitialEimOk.list.count - 1]->present) {
	case AddInitialEimResponse__addInitialEimOk__Member_PR_associationToken:
		euicc_result_data->choice.addEimResult.present = AddEimResult_PR_associationToken;
		euicc_result_data->choice.addEimResult.choice.associationToken =
		    add_init_eim_res->res->choice.addInitialEimOk.list.array[add_init_eim_res->res->
									     choice.addInitialEimOk.list.count -
									     1]->choice.associationToken;
		break;
	case AddInitialEimResponse__addInitialEimOk__Member_PR_addOk:
		euicc_result_data->choice.addEimResult.present = AddEimResult_PR_addEimResultCode;
		euicc_result_data->choice.addEimResult.choice.addEimResultCode = AddEimResult__addEimResultCode_ok;
		break;
	default:
		goto error;
	}

error:
	ipa_es10b_get_eim_cfg_data_free(eim_cfg_data);
	ipa_es10b_add_init_eim_res_free(add_init_eim_res);
	IPA_FREE(add_init_eim_req.req.eimConfigurationDataList.list.array);
	return euicc_result_data;
}

struct EuiccResultData *iot_emo_do_deleteEim_eco(struct ipa_context *ctx, const struct Eco__deleteEim *deleteEim_eco)
{
	struct EuiccResultData *euicc_result_data = IPA_ALLOC_ZERO(struct EuiccResultData);
	struct ipa_es10b_eim_cfg_data *eim_cfg_data = NULL;
	struct ipa_es10b_add_init_eim_req add_init_eim_req = { 0 };
	struct ipa_es10b_add_init_eim_res *add_init_eim_res = NULL;
	unsigned int i;
	struct EimConfigurationData *eim_cfg_data_item;
	bool eimFound = false;

	euicc_result_data->present = EuiccResultData_PR_deleteEimResult;
	euicc_result_data->choice.deleteEimResult = DeleteEimResult_undefinedError;

	/* Decode existing eIM configuration */
	eim_cfg_data = ipa_es10b_get_eim_cfg_data(ctx);
	if (!eim_cfg_data || !eim_cfg_data->res) {
		IPA_LOGP_ES10X("LoadEuiccPackage", LERROR,
			       "IoT eUICC emulation active, deleteEim eCO failed, unable to retrieve eimConfigurationData!\n");
		goto error;
	}

	/* Copy all existing eimConfiguration entries */
	for (i = 0; i < eim_cfg_data->res->eimConfigurationDataList.list.count; i++) {
		eim_cfg_data_item = eim_cfg_data->res->eimConfigurationDataList.list.array[i];

		/* Skip the item we want to delete */
		if (IPA_ASN_STR_CMP(&eim_cfg_data_item->eimId, &deleteEim_eco->eimId)) {
			euicc_result_data->choice.deleteEimResult = DeleteEimResult_ok;
			eimFound = true;
			continue;
		}

		ASN_SEQUENCE_ADD(&add_init_eim_req.req.eimConfigurationDataList.list, eim_cfg_data_item);
	}
	if (!eimFound) {
		euicc_result_data->choice.deleteEimResult = DeleteEimResult_eimNotFound;
		IPA_LOGP_ES10X("LoadEuiccPackage", LERROR,
			       "IoT eUICC emulation active, deleteEim eCO failed, unable to find eIM!\n");
		goto error;
	}

	/* Write new eIM configuration by executing ES10b:AddInitialEim. This will work since the IoT eUICC emulation
	 * does not check if there is already an eIM configuration in place. It will just overwrite the existing
	 * configuration with the one we have just assembled above. */
	add_init_eim_res = ipa_es10b_add_init_eim(ctx, &add_init_eim_req);
	if (!add_init_eim_res || add_init_eim_res->add_init_eim_err || !add_init_eim_res->res
	    || add_init_eim_res->res->present != AddInitialEimResponse_PR_addInitialEimOk) {
		IPA_LOGP_ES10X("LoadEuiccPackage", LERROR,
			       "IoT eUICC emulation active, deleteEim eCO failed, unable to write eimConfigurationData!\n");
		goto error;
	}

error:
	ipa_es10b_get_eim_cfg_data_free(eim_cfg_data);
	ipa_es10b_add_init_eim_res_free(add_init_eim_res);
	IPA_FREE(add_init_eim_req.req.eimConfigurationDataList.list.array);
	return euicc_result_data;
}

struct EuiccResultData *iot_emo_do_updateEim_eco(struct ipa_context *ctx,
						 const struct EimConfigurationData *updateEim_eco)
{
	struct EuiccResultData *euicc_result_data = IPA_ALLOC_ZERO(struct EuiccResultData);
	struct ipa_es10b_eim_cfg_data *eim_cfg_data = NULL;
	struct ipa_es10b_add_init_eim_req add_init_eim_req = { 0 };
	struct ipa_es10b_add_init_eim_res *add_init_eim_res = NULL;
	unsigned int i;
	struct EimConfigurationData *eim_cfg_data_item;
	struct EimConfigurationData *eim_cfg_data_item_updated;
	bool eimFound = false;

	euicc_result_data->present = EuiccResultData_PR_updateEimResult;
	euicc_result_data->choice.deleteEimResult = UpdateEimResult_undefinedError;

	/* Decode existing eIM configuration */
	eim_cfg_data = ipa_es10b_get_eim_cfg_data(ctx);
	if (!eim_cfg_data || !eim_cfg_data->res) {
		IPA_LOGP_ES10X("LoadEuiccPackage", LERROR,
			       "IoT eUICC emulation active, updateEim eCO failed, unable to retrieve eimConfigurationData!\n");
		goto error;
	}

	/* Copy all existing eimConfiguration entries */
	for (i = 0; i < eim_cfg_data->res->eimConfigurationDataList.list.count; i++) {
		eim_cfg_data_item = eim_cfg_data->res->eimConfigurationDataList.list.array[i];

		/* Modify the item we want to update */
		if (IPA_ASN_STR_CMP(&eim_cfg_data_item->eimId, &updateEim_eco->eimId) && !eimFound) {
			eim_cfg_data_item_updated = IPA_ALLOC_ZERO(struct EimConfigurationData);
			*eim_cfg_data_item_updated = *eim_cfg_data_item;

			/* In the following we will check which of the configuration parameters to update. It should be noted
			 * that it is not possible to update the eimId, which is obvious. However the spec also dictates that
			 * the updateEim eCO should not contain an associationToken. In case there is an associationToken
			 * anyway, we will silently ignore it. */
			if (updateEim_eco->eimFqdn)
				eim_cfg_data_item_updated->eimFqdn = updateEim_eco->eimFqdn;
			if (updateEim_eco->eimIdType)
				eim_cfg_data_item_updated->eimIdType = updateEim_eco->eimIdType;
			if (updateEim_eco->counterValue) {
				if (updateEim_eco->counterValue >= eim_cfg_data_item->counterValue)
					eim_cfg_data_item_updated->counterValue = updateEim_eco->counterValue;
				else if (updateEim_eco->eimPublicKeyData)
					eim_cfg_data_item_updated->counterValue = updateEim_eco->counterValue;
			}
			if (updateEim_eco->eimPublicKeyData)
				eim_cfg_data_item_updated->eimPublicKeyData = updateEim_eco->eimPublicKeyData;
			if (updateEim_eco->trustedPublicKeyDataTls)
				eim_cfg_data_item_updated->trustedPublicKeyDataTls =
				    updateEim_eco->trustedPublicKeyDataTls;
			if (updateEim_eco->eimSupportedProtocol)
				eim_cfg_data_item_updated->eimSupportedProtocol = updateEim_eco->eimSupportedProtocol;
			if (updateEim_eco->euiccCiPKId)
				eim_cfg_data_item_updated->euiccCiPKId = updateEim_eco->euiccCiPKId;

			euicc_result_data->choice.deleteEimResult = DeleteEimResult_ok;
			eimFound = true;
			ASN_SEQUENCE_ADD(&add_init_eim_req.req.eimConfigurationDataList.list,
					 eim_cfg_data_item_updated);
		} else {
			ASN_SEQUENCE_ADD(&add_init_eim_req.req.eimConfigurationDataList.list, eim_cfg_data_item);
		}
	}
	if (!eimFound) {
		euicc_result_data->choice.deleteEimResult = UpdateEimResult_eimNotFound;
		IPA_LOGP_ES10X("LoadEuiccPackage", LERROR,
			       "IoT eUICC emulation active, updateEim eCO failed, unable to find eIM!\n");
		goto error;
	}

	/* Write new eIM configuration by executing ES10b:AddInitialEim. This will work since the IoT eUICC emulation
	 * does not check if there is already an eIM configuration in place. It will just overwrite the existing
	 * configuration with the one we have just assembled above. */
	add_init_eim_res = ipa_es10b_add_init_eim(ctx, &add_init_eim_req);
	if (!add_init_eim_res || add_init_eim_res->add_init_eim_err || !add_init_eim_res->res
	    || add_init_eim_res->res->present != AddInitialEimResponse_PR_addInitialEimOk) {
		IPA_LOGP_ES10X("LoadEuiccPackage", LERROR,
			       "IoT eUICC emulation active, updateEim eCO failed, unable to write eimConfigurationData!\n");
		goto error;
	}

error:
	ipa_es10b_get_eim_cfg_data_free(eim_cfg_data);
	ipa_es10b_add_init_eim_res_free(add_init_eim_res);
	IPA_FREE(add_init_eim_req.req.eimConfigurationDataList.list.array);
	IPA_FREE(eim_cfg_data_item_updated);
	return euicc_result_data;
}

struct EuiccResultData *iot_emo_do_listEim_eco(struct ipa_context *ctx, const struct Eco__listEim *listEim_eco)
{
	struct EuiccResultData *euicc_result_data = IPA_ALLOC_ZERO(struct EuiccResultData);
	struct ipa_es10b_eim_cfg_data *eim_cfg_data = NULL;
	struct EimIdInfo *eim_id_info;
	struct EimConfigurationData *eim_cfg_data_item;
	unsigned int i;

	euicc_result_data->present = EuiccResultData_PR_listEimResult;

	/* This eCO Has no parameters, so listEim_eco is just an empty struct that has to be present */
	assert(listEim_eco);

	eim_cfg_data = ipa_es10b_get_eim_cfg_data(ctx);
	if (!eim_cfg_data) {
		euicc_result_data->choice.listEimResult.present = ListEimResult_PR_listEimError;
		euicc_result_data->choice.listEimResult.choice.listEimError = ListEimResult__listEimError_commandError;
	} else {
		euicc_result_data->choice.listEimResult.present = ListEimResult_PR_eimIdList;

		for (i = 0; i < eim_cfg_data->res->eimConfigurationDataList.list.count; i++) {
			eim_cfg_data_item = eim_cfg_data->res->eimConfigurationDataList.list.array[i];
			eim_id_info = IPA_ALLOC_ZERO(struct EimIdInfo);
			assert(eim_id_info);

			/* Copy eimId */
			eim_id_info->eimId.size = eim_cfg_data_item->eimId.size;
			eim_id_info->eimId.buf = IPA_ALLOC_N(eim_cfg_data_item->eimId.size);
			assert(eim_id_info->eimId.buf);
			memcpy(eim_id_info->eimId.buf, eim_cfg_data_item->eimId.buf, eim_cfg_data_item->eimId.size);

			/* Copy eimIdType */
			if (eim_cfg_data_item->eimIdType) {
				eim_id_info->eimIdType = IPA_ALLOC_N(sizeof(eim_cfg_data_item->eimIdType));
				assert(eim_id_info->eimIdType);
				*eim_id_info->eimIdType = *eim_cfg_data_item->eimIdType;
			}

			ASN_SEQUENCE_ADD(&euicc_result_data->choice.listEimResult.choice.eimIdList.list, eim_id_info);
		}
	}

	ipa_es10b_get_eim_cfg_data_free(eim_cfg_data);
	return euicc_result_data;
}

struct ipa_es10b_load_euicc_pkg_res *load_euicc_pkg_iot_emu(struct ipa_context *ctx,
							    const struct ipa_es10b_load_euicc_pkg_req *req)
{
	struct ipa_es10b_load_euicc_pkg_res *res = NULL;
	struct EuiccPackageResult *asn = NULL;
	const struct Psmo *psmo = NULL;
	struct EuiccResultData *psmo_result = NULL;
	const struct Eco *eco = NULL;
	struct EuiccResultData *eco_result = NULL;
	struct ipa_buf eim_id = { 0 };
	struct ipa_buf euicc_sign_epr = { 0 };
	unsigned int i;
	const uint8_t euiccSignEPR_dummy[64] = { "RRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRRSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSSS" };

	IPA_LOGP_ES10X("LoadEuiccPackage", LINFO,
		       "IoT eUICC emulation active, executing ECOs and PSMOs by calling equivalent consumer eUICC ES10x functions...\n");

	/* Before executing an eUICC package, ensure that the rollback ICCID is up to date. */
	update_rollback_iccid(ctx);

	/* Setup an (emulated) EuiccPackageResult */
	res = IPA_ALLOC_ZERO(struct ipa_es10b_load_euicc_pkg_res);
	asn = IPA_ALLOC_ZERO(struct EuiccPackageResult);
	res->res = asn;
	asn->present = EuiccPackageResult_PR_euiccPackageResultSigned;
	ipa_buf_assign(&eim_id, (uint8_t *) ctx->eim_id, strlen(ctx->eim_id));
	IPA_COPY_IPA_BUF_TO_ASN(&asn->choice.euiccPackageResultSigned.euiccPackageResultDataSigned.eimId, &eim_id);
	asn->choice.euiccPackageResultSigned.euiccPackageResultDataSigned.counterValue =
	    req->req.euiccPackageSigned.counterValue;
	if (req->req.euiccPackageSigned.transactionId) {
		asn->choice.euiccPackageResultSigned.euiccPackageResultDataSigned.transactionId =
		    ipa_asn1c_dup(&asn_DEF_TransactionId, req->req.euiccPackageSigned.transactionId);
	}
	asn->choice.euiccPackageResultSigned.euiccPackageResultDataSigned.seqNumber = 0;
	ipa_buf_assign(&euicc_sign_epr, euiccSignEPR_dummy, sizeof(euiccSignEPR_dummy));
	IPA_COPY_IPA_BUF_TO_ASN(&asn->choice.euiccPackageResultSigned.euiccSignEPR, &euicc_sign_epr);

	/* Go through the list of PSMOs and ECOs and execute the corresponding iot_emo_do... functions */
	switch (req->req.euiccPackageSigned.euiccPackage.present) {
	case EuiccPackage_PR_psmoList:
		for (i = 0; i < req->req.euiccPackageSigned.euiccPackage.choice.psmoList.list.count; i++) {
			psmo = req->req.euiccPackageSigned.euiccPackage.choice.psmoList.list.array[i];
			psmo_result = NULL;
			switch (psmo->present) {
			case Psmo_PR_enable:
				psmo_result = iot_emo_do_enable_psmo(ctx, &psmo->choice.enable);
				break;
			case Psmo_PR_disable:
				psmo_result = iot_emo_do_disable_psmo(ctx, &psmo->choice.disable);
				break;
			case Psmo_PR_delete:
				psmo_result = iot_emo_do_delete_psmo(ctx, &psmo->choice.Delete);
				break;
			case Psmo_PR_listProfileInfo:
				psmo_result = iot_emo_do_listProfileInfo_psmo(ctx, &psmo->choice.listProfileInfo);
				break;
			case Psmo_PR_getRAT:
				psmo_result = iot_emo_do_getRAT_psmo(ctx, &psmo->choice.getRAT);
				break;
			case Psmo_PR_configureAutoEnable:
				psmo_result =
				    iot_emo_do_configureAutoEnable_psmo(ctx, &psmo->choice.configureAutoEnable);
				break;
			default:
				IPA_LOGP_ES10X("LoadEuiccPackage", LERROR, "ignoring invalid or unsupported PSMO!\n");
				break;
			}
			if (psmo_result)
				ASN_SEQUENCE_ADD(&asn->choice.euiccPackageResultSigned.euiccPackageResultDataSigned.
						 euiccResult.list, psmo_result);
		}
		break;
	case EuiccPackage_PR_ecoList:
		for (i = 0; i < req->req.euiccPackageSigned.euiccPackage.choice.ecoList.list.count; i++) {
			eco = req->req.euiccPackageSigned.euiccPackage.choice.ecoList.list.array[i];
			eco_result = NULL;
			switch (eco->present) {
			case Eco_PR_addEim:
				eco_result = iot_emo_do_addEim_eco(ctx, &eco->choice.addEim);
				break;
			case Eco_PR_deleteEim:
				eco_result = iot_emo_do_deleteEim_eco(ctx, &eco->choice.deleteEim);
				break;
			case Eco_PR_updateEim:
				eco_result = iot_emo_do_updateEim_eco(ctx, &eco->choice.updateEim);
				break;
			case Eco_PR_listEim:
				eco_result = iot_emo_do_listEim_eco(ctx, &eco->choice.listEim);
				break;
			default:
				IPA_LOGP_ES10X("LoadEuiccPackage", LERROR, "ignoring invalid or unsupported eCO!\n");
				break;
			}
			if (eco_result)
				ASN_SEQUENCE_ADD(&asn->choice.euiccPackageResultSigned.euiccPackageResultDataSigned.
						 euiccResult.list, eco_result);
		}
		break;
	default:
		IPA_LOGP_ES10X("LoadEuiccPackage", LERROR,
			       "the eUICC package contained neither a psmoList, nor an ecoList!\n");
		goto error;
	}

	return res;
error:
	ipa_es10b_load_euicc_pkg_res_free(res);
	res = IPA_ALLOC_ZERO(struct ipa_es10b_load_euicc_pkg_res);
	asn = IPA_ALLOC_ZERO(struct EuiccPackageResult);
	res->res = asn;
	asn->present = EuiccPackageResult_PR_euiccPackageErrorUnsigned;
	ipa_buf_assign(&eim_id, (uint8_t *) ctx->eim_id, strlen(ctx->eim_id));
	IPA_COPY_IPA_BUF_TO_ASN(&asn->choice.euiccPackageErrorUnsigned.eimId, &eim_id);
	return res;
}

/* Check if the euicc package that we have just executed has done any changes to the currently selected profile */
static bool check_for_profile_change(const struct EuiccPackageResult *res)
{
	unsigned int i;
	struct EuiccResultData *euicc_result_data;
	if (!res)
		return false;
	if (res->present != EuiccPackageResult_PR_euiccPackageResultSigned)
		return false;

	for (i = 0; i < res->choice.euiccPackageResultSigned.euiccPackageResultDataSigned.euiccResult.list.count; i++) {
		euicc_result_data =
		    res->choice.euiccPackageResultSigned.euiccPackageResultDataSigned.euiccResult.list.array[i];
		switch (euicc_result_data->present) {
		case EuiccResultData_PR_enableResult:
			if (euicc_result_data->choice.enableResult == EnableProfileResult_ok)
				return true;
			break;
		case EuiccResultData_PR_disableResult:
			if (euicc_result_data->choice.disableResult == DisableProfileResult_ok)
				return true;
			break;
		case EuiccResultData_PR_rollbackResult:
			if (euicc_result_data->choice.rollbackResult == RollbackProfileResult_ok)
				return true;
			break;
		default:
			break;
		}
	}

	return false;
}

/* Check if the euicc package contains an enable PSMO that has the rollback flag set */
static bool check_for_rollback_flag(const struct EuiccPackageRequest *req)
{
	unsigned int i;
	struct Psmo *psmo;
	if (!req)
		return false;
	if (req->euiccPackageSigned.euiccPackage.present != EuiccPackage_PR_psmoList)
		return false;

	for (i = 0; i < req->euiccPackageSigned.euiccPackage.choice.psmoList.list.count; i++) {
		psmo = req->euiccPackageSigned.euiccPackage.choice.psmoList.list.array[i];

		if (psmo->present == Psmo_PR_enable && psmo->choice.enable.rollbackFlag)
			return true;
	}

	return false;
}

/*! Function (ES10b): LoadEuiccPackage.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10b_load_euicc_pkg_res *ipa_es10b_load_euicc_pkg(struct ipa_context *ctx,
							      const struct ipa_es10b_load_euicc_pkg_req *req)
{
	struct ipa_es10b_load_euicc_pkg_res *res;

	if (ctx->cfg->iot_euicc_emu_enabled)
		res = load_euicc_pkg_iot_emu(ctx, req);
	else
		res = load_euicc_pkg(ctx, req);

	if (!res)
		return NULL;

	res->profile_changed = check_for_profile_change(res->res);
	res->rollback_allowed = check_for_rollback_flag(&req->req);

	return res;
}

/*! Free results of function (ES10b): LoadEuiccPackage.
 *  \param[in] res pointer to function result. */
void ipa_es10b_load_euicc_pkg_res_free(struct ipa_es10b_load_euicc_pkg_res *res)
{
	IPA_ES10X_RES_FREE(asn_DEF_EuiccPackageResult, res);
}
