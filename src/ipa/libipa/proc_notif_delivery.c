/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.32, section 3.7: Notification Delivery to Notification Receivers
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/log.h>
#include "context.h"
#include "utils.h"
#include "proc_notif_delivery.h"
#include "es10b_retr_notif_from_lst.h"
#include "esipa_handle_notif.h"
#include "es10b_rm_notif_from_lst.h"

/* Extract the seqNumber from a PendingNotification. Returns 0 on success, negative when the
 * notification type is unknown (then the seqNumber cannot be determined). */
static int pending_notif_seq_number(const struct SGP32_PendingNotification *notif, long *seq_number)
{
	switch (notif->present) {
	case SGP32_PendingNotification_PR_profileInstallationResult:
		*seq_number =
		    notif->choice.profileInstallationResult.profileInstallationResultData.notificationMetadata.
		    seqNumber;
		return 0;
	case SGP32_PendingNotification_PR_otherSignedNotification:
		*seq_number = notif->choice.otherSignedNotification.tbsOtherNotification.seqNumber;
		return 0;
	default:
		/* This should not happen, the eUICC should only return the two notification types listed above */
		return -EINVAL;
	}
}

/*! Perform Notification Delivery to Notification Receivers Procedure.
 *  \param[inout] ctx pointer to ipa_context.
 *  \returns 0 on success, negative on failure. */
int ipa_notif_delivery(struct ipa_context *ctx)
{
	struct ipa_es10b_retr_notif_from_lst_req retr_notif_from_lst_req = { 0 };
	struct ipa_es10b_retr_notif_from_lst_res *retr_notif_from_lst_res = NULL;
	struct ipa_esipa_handle_notif_req handle_notif_req = { 0 };
	long *seq_numbers = NULL;
	unsigned int seq_number_count = 0;
	unsigned int i;
	int rc;

	/* The pending notifications are retrieved from the eUICC one by one (searchCriteria seqNumber),
	 * so that only a single notification is decoded in memory while the (potentially many) HTTP
	 * deliveries run. The full list is read once, but only to learn the sequence numbers. */
	retr_notif_from_lst_res = ipa_es10b_retr_notif_from_lst(ctx, &retr_notif_from_lst_req);
	if (!retr_notif_from_lst_res)
		goto error;
	else if (retr_notif_from_lst_res->notif_lst_result_err)
		goto error;
	else if (!retr_notif_from_lst_res->sgp32_res)
		goto error;

	/* In this procedure we expect to get a notificationList, all other types of lists are not suitable for this
	 * procedure. */
	else if (retr_notif_from_lst_res->sgp32_res->present !=
		 SGP32_RetrieveNotificationsListResponse_PR_notificationList) {
		IPA_LOGP(SIPA, LERROR, "Expecting a notificationList, but got something different!\n");
		goto error;
	}

	if (retr_notif_from_lst_res->sgp32_res->choice.notificationList.list.count) {
		seq_numbers = IPA_ALLOC_N(sizeof(*seq_numbers) *
					  retr_notif_from_lst_res->sgp32_res->choice.notificationList.list.count);
		assert(seq_numbers);
		for (i = 0; i < retr_notif_from_lst_res->sgp32_res->choice.notificationList.list.count; i++) {
			rc = pending_notif_seq_number(retr_notif_from_lst_res->sgp32_res->
						      choice.notificationList.list.array[i],
						      &seq_numbers[seq_number_count]);
			if (rc < 0) {
				IPA_LOGP(SIPA, LERROR,
					 "Unknown type of notification, skipping notification No.%u\n", i);
				continue;
			}
			seq_number_count++;
		}
	}

	ipa_es10b_retr_notif_from_lst_res_free(retr_notif_from_lst_res);
	retr_notif_from_lst_res = NULL;

	for (i = 0; i < seq_number_count; i++) {
		IPA_LOGP(SIPA, LERROR, "Delivery of notification No.%u (seqNumber %ld):\n", i, seq_numbers[i]);

		retr_notif_from_lst_req.search_criteria.present =
		    SGP32_RetrieveNotificationsListRequest__searchCriteria_PR_seqNumber;
		retr_notif_from_lst_req.search_criteria.choice.seqNumber = seq_numbers[i];
		retr_notif_from_lst_res = ipa_es10b_retr_notif_from_lst(ctx, &retr_notif_from_lst_req);
		if (!retr_notif_from_lst_res || retr_notif_from_lst_res->notif_lst_result_err ||
		    !retr_notif_from_lst_res->sgp32_res ||
		    retr_notif_from_lst_res->sgp32_res->present !=
		    SGP32_RetrieveNotificationsListResponse_PR_notificationList ||
		    retr_notif_from_lst_res->sgp32_res->choice.notificationList.list.count < 1) {
			IPA_LOGP(SIPA, LERROR,
				 "Retrieval of notification No.%u failed, will try again later!\n", i);
			ipa_es10b_retr_notif_from_lst_res_free(retr_notif_from_lst_res);
			retr_notif_from_lst_res = NULL;
			continue;
		}

		handle_notif_req.pending_notification =
		    retr_notif_from_lst_res->sgp32_res->choice.notificationList.list.array[0];
		rc = ipa_esipa_handle_notif(ctx, &handle_notif_req);
		if (rc < 0)
			IPA_LOGP(SIPA, LERROR, "Delivery of notification No.%u failed, will try again later!\n", i);
		else
			ipa_es10b_rm_notif_from_lst(ctx, seq_numbers[i]);

		ipa_es10b_retr_notif_from_lst_res_free(retr_notif_from_lst_res);
		retr_notif_from_lst_res = NULL;
	}

	IPA_FREE(seq_numbers);
	IPA_LOGP(SIPA, LDEBUG, "Notification Delivery to Notification Receivers Procedure succeeded!\n");
	return 0;
error:
	IPA_LOGP(SIPA, LERROR, "Notification Delivery to Notification Receivers Procedure failed!\n");
	ipa_es10b_retr_notif_from_lst_res_free(retr_notif_from_lst_res);
	return -EINVAL;
}
