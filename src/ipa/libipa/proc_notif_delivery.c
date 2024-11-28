/*
 * Author: Philipp Maier
 * See also: GSMA SGP.32, section 3.7: Notification Delivery to Notification Receivers
 *
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

/*! Perform Notification Delivery to Notification Receivers Procedure.
 *  \param[inout] ctx pointer to ipa_context.
 *  \returns 0 on success, negative on failure. */
int ipa_notif_delivery(struct ipa_context *ctx)
{
	struct ipa_es10b_retr_notif_from_lst_req retr_notif_from_lst_req = { 0 };
	struct ipa_es10b_retr_notif_from_lst_res *retr_notif_from_lst_res = NULL;
	struct ipa_esipa_handle_notif_req handle_notif_req = { 0 };
	unsigned int i;
	int rc;
	long seq_number;

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

	for (i = 0; i < retr_notif_from_lst_res->sgp32_res->choice.notificationList.list.count; i++) {
		IPA_LOGP(SIPA, LERROR, "Delivery of notification No.%u:\n", i);
		handle_notif_req.pending_notification =
		    retr_notif_from_lst_res->sgp32_res->choice.notificationList.list.array[i];
		rc = ipa_esipa_handle_notif(ctx, &handle_notif_req);
		if (rc < 0)
			IPA_LOGP(SIPA, LERROR, "Delivery of notification No.%u failed, will try again later!\n", i);
		else {
			switch (handle_notif_req.pending_notification->present) {
			case SGP32_PendingNotification_PR_profileInstallationResult:
				seq_number =
				    handle_notif_req.pending_notification->choice.profileInstallationResult.
				    profileInstallationResultData.notificationMetadata.seqNumber;
				ipa_es10b_rm_notif_from_lst(ctx, seq_number);
				break;
			case SGP32_PendingNotification_PR_otherSignedNotification:
				seq_number =
				    handle_notif_req.pending_notification->choice.otherSignedNotification.
				    tbsOtherNotification.seqNumber;
				ipa_es10b_rm_notif_from_lst(ctx, seq_number);
				break;
			default:
				/* This should not happen, the eUICC should only return the two notification types listed above */
				IPA_LOGP(SIPA, LERROR,
					 "Unknown type of notification, removal of notification No.%u failed\n", i);
			}
		}
	}

	IPA_LOGP(SIPA, LDEBUG, "Notification Delivery to Notification Receivers Procedure succeeded!\n");
	ipa_es10b_retr_notif_from_lst_res_free(retr_notif_from_lst_res);
	return 0;
error:
	IPA_LOGP(SIPA, LERROR, "Notification Delivery to Notification Receivers Procedure failed!\n");
	ipa_es10b_retr_notif_from_lst_res_free(retr_notif_from_lst_res);
	return -EINVAL;
}
