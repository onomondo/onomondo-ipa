/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, 5.7.11: Function (ES10b): RemoveNotificationFromList
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <NotificationSentRequest.h>
#include <NotificationSentResponse.h>
#include <Octet16.h>
#include "context.h"
#include "length.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_get_euicc_chlg.h"

static const struct num_str_map error_code_strings[] = {
	{ NotificationSentResponse__deleteNotificationStatus_ok, "ok" },
	{ NotificationSentResponse__deleteNotificationStatus_nothingToDelete, "nothingToDelete" },
	{ NotificationSentResponse__deleteNotificationStatus_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_notif_sent_resp(const struct ipa_buf *es10b_res)
{
	struct NotificationSentResponse *asn = NULL;
	int rc = 0;

	asn = ipa_es10x_res_dec(&asn_DEF_NotificationSentResponse, es10b_res, "RemoveNotificationFromList");
	if (!asn)
		return -EINVAL;

	if (asn->deleteNotificationStatus != NotificationSentResponse__deleteNotificationStatus_ok &&
	    asn->deleteNotificationStatus != NotificationSentResponse__deleteNotificationStatus_nothingToDelete) {
		IPA_LOGP_ES10X("RemoveNotificationFromList", LERROR, "function failed with status code %ld=%s!\n",
			       asn->deleteNotificationStatus, ipa_str_from_num(error_code_strings,
									       asn->deleteNotificationStatus,
									       "(unknown)"));
		rc = -EINVAL;
	} else {
		IPA_LOGP_ES10X("RemoveNotificationFromList", LERROR, "function succeeded with status code %ld=%s!\n",
			       asn->deleteNotificationStatus, ipa_str_from_num(error_code_strings,
									       asn->deleteNotificationStatus,
									       "(unknown)"));
	}

	ASN_STRUCT_FREE(asn_DEF_NotificationSentResponse, asn);

	return rc;
}

/*! Function (ES10b): RemoveNotificationFromList.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] seq_number sequence number to identify the notification.
 *  \returns 0 on success, negative on error. */
int ipa_es10b_rm_notif_from_lst(struct ipa_context *ctx, long seq_number)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct NotificationSentRequest notif_sent_req = { 0 };
	int rc = -EINVAL;

	notif_sent_req.seqNumber = seq_number;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_NotificationSentRequest, &notif_sent_req, "RemoveNotificationFromList");
	if (!es10b_req) {
		IPA_LOGP_ES10X("RemoveNotificationFromList", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("RemoveNotificationFromList", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_notif_sent_resp(es10b_res);
	if (rc < 0)
		goto error;

error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return rc;
}
