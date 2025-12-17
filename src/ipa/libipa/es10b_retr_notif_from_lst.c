/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, 5.7.10: Function (ES10b): RetrieveNotificationsList
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
#include "esipa.h"
#include "es10b_retr_notif_from_lst.h"

/* Convert a notificationList (RetrieveNotificationsListResponse) from RSP to SGP32 format. */
void convert_notification_list(struct SGP32_RetrieveNotificationsListResponse__notificationList *lst_out,
			       const struct RetrieveNotificationsListResponse__notificationList *lst_in)
{
	unsigned int i;
	struct PendingNotification *pending_notif_item;
	struct SGP32_PendingNotification *sgp32_pending_notif_item;

	OtherSignedNotification_t *other_signed_notification;
	ProfileInstallationResultData_t *profile_Installation_result_data;
	EuiccSignPIR_t *euicc_sign_PIR;

	for (i = 0; i < lst_in->list.count; i++) {
		pending_notif_item = lst_in->list.array[i];

		switch (pending_notif_item->present) {
		case PendingNotification_PR_profileInstallationResult:
			profile_Installation_result_data =
			    &pending_notif_item->choice.profileInstallationResult.profileInstallationResultData;
			euicc_sign_PIR = &pending_notif_item->choice.profileInstallationResult.euiccSignPIR;

			sgp32_pending_notif_item = IPA_ALLOC_ZERO(struct SGP32_PendingNotification);
			ASN_SEQUENCE_ADD(&lst_out->list, sgp32_pending_notif_item);
			sgp32_pending_notif_item->present = SGP32_PendingNotification_PR_profileInstallationResult;
			sgp32_pending_notif_item->choice.profileInstallationResult.profileInstallationResultData =
			    *profile_Installation_result_data;
			sgp32_pending_notif_item->choice.profileInstallationResult.euiccSignPIR = *euicc_sign_PIR;
			break;
		case PendingNotification_PR_otherSignedNotification:
			other_signed_notification = &pending_notif_item->choice.otherSignedNotification;

			sgp32_pending_notif_item = IPA_ALLOC_ZERO(struct SGP32_PendingNotification);
			ASN_SEQUENCE_ADD(&lst_out->list, sgp32_pending_notif_item);
			sgp32_pending_notif_item->present = SGP32_PendingNotification_PR_otherSignedNotification;
			sgp32_pending_notif_item->choice.otherSignedNotification = *other_signed_notification;
			break;
		default:
			IPA_LOGP_ESIPA("ProvideEimPackageResult", LERROR, "skipping empty PendingNotification item\n");
			break;
		}
	}
}

/*! Free a converted notificationList (RetrieveNotificationsListResponse). */
void free_converted_notification_list(struct SGP32_RetrieveNotificationsListResponse__notificationList *lst)
{
	int i;
	if (!lst)
		return;
	for (i = 0; i < lst->list.count; i++)
		IPA_FREE(lst->list.array[i]);
	IPA_FREE(lst->list.array);
}

static const struct num_str_map error_code_strings[] = {
	{ RetrieveNotificationsListResponse__notificationsListResultError_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static const struct num_str_map error_code_strings_sgp32[] = {
	{ SGP32_RetrieveNotificationsListResponse__notificationsListResultError_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_retr_notif_from_lst_res(struct ipa_es10b_retr_notif_from_lst_res *res, const struct ipa_buf *es10b_res)
{
	struct RetrieveNotificationsListResponse *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_RetrieveNotificationsListResponse, es10b_res, "RetrieveNotificationsList");
	if (!asn)
		return -EINVAL;

	switch (asn->present) {
	case RetrieveNotificationsListResponse_PR_notificationList:
		res->sgp32_res = IPA_ALLOC_ZERO(struct SGP32_RetrieveNotificationsListResponse);
		res->sgp32_res->present = SGP32_RetrieveNotificationsListResponse_PR_notificationList;
		convert_notification_list(&res->sgp32_res->choice.notificationList, &asn->choice.notificationList);
		break;
	case RetrieveNotificationsListResponse_PR_notificationsListResultError:
		res->sgp32_res = IPA_ALLOC_ZERO(struct SGP32_RetrieveNotificationsListResponse);
		res->sgp32_res->present = SGP32_RetrieveNotificationsListResponse_PR_notificationsListResultError;
		res->sgp32_res->choice.notificationsListResultError = asn->choice.notificationsListResultError;

		res->notif_lst_result_err = asn->choice.notificationsListResultError;
		IPA_LOGP_ES10X("RetrieveNotificationsList", LERROR, "function failed with error code %ld=%s!\n",
			       res->notif_lst_result_err, ipa_str_from_num(error_code_strings,
									   res->notif_lst_result_err, "(unknown)"));
		break;
	default:
		IPA_LOGP_ES10X("RetrieveNotificationsList", LERROR, "unexpected response content!\n");
		res->notif_lst_result_err = -1;
	}

	res->res = asn;
	return 0;
}

static int dec_retr_notif_from_lst_res_sgp32(struct ipa_es10b_retr_notif_from_lst_res *res,
					     const struct ipa_buf *es10b_res)
{
	struct SGP32_RetrieveNotificationsListResponse *asn = NULL;

	asn =
	    ipa_es10x_res_dec(&asn_DEF_SGP32_RetrieveNotificationsListResponse, es10b_res, "RetrieveNotificationsList");
	if (!asn)
		return -EINVAL;

	switch (asn->present) {
	case SGP32_RetrieveNotificationsListResponse_PR_notificationsListResultError:
		res->notif_lst_result_err = asn->choice.notificationsListResultError;
		IPA_LOGP_ES10X("RetrieveNotificationsList", LERROR, "function failed with error code %ld=%s!\n",
			       res->notif_lst_result_err, ipa_str_from_num(error_code_strings_sgp32,
									   res->notif_lst_result_err, "(unknown)"));
	case SGP32_RetrieveNotificationsListResponse_PR_notificationList:
	case SGP32_RetrieveNotificationsListResponse_PR_euiccPackageResultList:
	case SGP32_RetrieveNotificationsListResponse_PR_notificationAndEprList:
		/* Nothing to do */
		break;
	default:
		IPA_LOGP_ES10X("RetrieveNotificationsList", LERROR, "unexpected response content!\n");
		res->notif_lst_result_err = -1;
	}

	res->sgp32_res = asn;
	return 0;
}

static struct ipa_buf *enc_retr_notif_from_lst_req(const struct ipa_es10b_retr_notif_from_lst_req *req)
{
	struct RetrieveNotificationsListRequest asn = { 0 };
	struct RetrieveNotificationsListRequest__searchCriteria search_criteria = { 0 };
	struct ipa_buf *es10b_req = NULL;

	if (req->dr_search_criteria) {
		/* Convert from foreigen searchCriteria (see comment in header file) */
		switch (req->dr_search_criteria->present) {
		case IpaEuiccDataRequest__searchCriteria_PR_seqNumber:
			search_criteria.present = RetrieveNotificationsListRequest__searchCriteria_PR_seqNumber;
			search_criteria.choice.seqNumber = req->dr_search_criteria->choice.seqNumber;
			break;
		case IpaEuiccDataRequest__searchCriteria_PR_profileManagementOperation:
			search_criteria.present =
			    RetrieveNotificationsListRequest__searchCriteria_PR_profileManagementOperation;
			search_criteria.choice.profileManagementOperation =
			    req->dr_search_criteria->choice.profileManagementOperation;
			break;
		case IpaEuiccDataRequest__searchCriteria_PR_euiccPackageResults:
			IPA_LOGP_ES10X("RetrieveNotificationsList", LERROR,
				       "unsupported euiccPackageResults searchCriteria in IpaEuiccDataRequest!\n");
			search_criteria.present = RetrieveNotificationsListRequest__searchCriteria_PR_NOTHING;
			break;
		default:
			IPA_LOGP_ES10X("RetrieveNotificationsList", LERROR,
				       "empty searchCriteria in IpaEuiccDataRequest!\n");
			search_criteria.present = RetrieveNotificationsListRequest__searchCriteria_PR_NOTHING;
			break;
		}
	} else {
		/* Use native search_criteria as provided by caller */
		search_criteria.present = req->search_criteria.present;
		switch (req->search_criteria.present) {
		case SGP32_RetrieveNotificationsListRequest__searchCriteria_PR_seqNumber:
			search_criteria.choice.seqNumber = req->search_criteria.choice.seqNumber;
			break;
		case SGP32_RetrieveNotificationsListRequest__searchCriteria_PR_profileManagementOperation:
			search_criteria.choice.profileManagementOperation =
			    req->search_criteria.choice.profileManagementOperation;
			break;
		case SGP32_RetrieveNotificationsListRequest__searchCriteria_PR_euiccPackageResults:
			IPA_LOGP_ES10X("RetrieveNotificationsList", LERROR,
				       "unsupported euiccPackageResults searchCriteria!\n");
			search_criteria.present = RetrieveNotificationsListRequest__searchCriteria_PR_NOTHING;
			break;
		default:
			IPA_LOGP_ES10X("RetrieveNotificationsList", LERROR, "empty searchCriteria!\n");
			search_criteria.present = RetrieveNotificationsListRequest__searchCriteria_PR_NOTHING;
			break;
		}
	}

	if (search_criteria.present != RetrieveNotificationsListRequest__searchCriteria_PR_NOTHING)
		asn.searchCriteria = &search_criteria;
	es10b_req = ipa_es10x_req_enc(&asn_DEF_RetrieveNotificationsListRequest, &asn, "RetrieveNotificationsList");

	return es10b_req;
}

static struct ipa_buf *enc_retr_notif_from_lst_req_sgp32(const struct ipa_es10b_retr_notif_from_lst_req *req)
{
	struct SGP32_RetrieveNotificationsListRequest asn = { 0 };
	struct SGP32_RetrieveNotificationsListRequest__searchCriteria search_criteria = { 0 };
	struct ipa_buf *es10b_req = NULL;

	if (req->dr_search_criteria) {
		/* Convert from foreigen searchCriteria (see comment in header file) */
		switch (req->dr_search_criteria->present) {
		case IpaEuiccDataRequest__searchCriteria_PR_seqNumber:
			search_criteria.present = SGP32_RetrieveNotificationsListRequest__searchCriteria_PR_seqNumber;
			search_criteria.choice.seqNumber = req->dr_search_criteria->choice.seqNumber;
			break;
		case IpaEuiccDataRequest__searchCriteria_PR_profileManagementOperation:
			search_criteria.present =
			    SGP32_RetrieveNotificationsListRequest__searchCriteria_PR_profileManagementOperation;
			search_criteria.choice.profileManagementOperation =
			    req->dr_search_criteria->choice.profileManagementOperation;
			break;
		case IpaEuiccDataRequest__searchCriteria_PR_euiccPackageResults:
			search_criteria.present =
			    SGP32_RetrieveNotificationsListRequest__searchCriteria_PR_euiccPackageResults;
			search_criteria.choice.euiccPackageResults =
			    req->dr_search_criteria->choice.euiccPackageResults;
			break;
		default:
			IPA_LOGP_ES10X("RetrieveNotificationsList", LERROR,
				       "empty searchCriteria in IpaEuiccDataRequest!\n");
			search_criteria.present = RetrieveNotificationsListRequest__searchCriteria_PR_NOTHING;
			break;
		}
	} else {
		/* Use native search_criteria as provided by caller */
		search_criteria = req->search_criteria;
	}

	if (search_criteria.present != SGP32_RetrieveNotificationsListRequest__searchCriteria_PR_NOTHING)
		asn.searchCriteria = &search_criteria;
	es10b_req =
	    ipa_es10x_req_enc(&asn_DEF_SGP32_RetrieveNotificationsListRequest, &asn, "RetrieveNotificationsList");

	return es10b_req;
}

/*! Function (ES10b): RetrieveNotificationsList.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10b_retr_notif_from_lst_res *ipa_es10b_retr_notif_from_lst(struct ipa_context *ctx, const struct ipa_es10b_retr_notif_from_lst_req
									*req)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ipa_es10b_retr_notif_from_lst_res *res = IPA_ALLOC_ZERO(struct ipa_es10b_retr_notif_from_lst_res);
	int rc;

	if (ctx->cfg->iot_euicc_emu_enabled)
		es10b_req = enc_retr_notif_from_lst_req(req);
	else
		es10b_req = enc_retr_notif_from_lst_req_sgp32(req);
	if (!es10b_req) {
		IPA_LOGP_ES10X("RetrieveNotificationsList", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("RetrieveNotificationsList", LERROR, "no ES10b response\n");
		goto error;
	}

	if (ctx->cfg->iot_euicc_emu_enabled) {
		IPA_LOGP_ES10X("RetrieveNotificationsList", LINFO,
			       "IoT eUICC emulation active, will derive notificationList from (SGP.22) RetrieveNotificationsListResponse.\n");
		rc = dec_retr_notif_from_lst_res(res, es10b_res);
	} else {
		rc = dec_retr_notif_from_lst_res_sgp32(res, es10b_res);
	}
	if (rc < 0)
		goto error;

	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return res;
error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	ipa_es10b_retr_notif_from_lst_res_free(res);
	return NULL;
}

void ipa_es10b_retr_notif_from_lst_res_free(struct ipa_es10b_retr_notif_from_lst_res *res)
{
	if (!res)
		return;

	if (res->res) {
		free_converted_notification_list(&res->sgp32_res->choice.notificationList);
		IPA_FREE(res->sgp32_res);
		ASN_STRUCT_FREE(asn_DEF_RetrieveNotificationsListResponse, res->res);
	} else if (res->sgp32_res) {
		ASN_STRUCT_FREE(asn_DEF_SGP32_RetrieveNotificationsListResponse, res->sgp32_res);
	}

	IPA_FREE(res);
}
