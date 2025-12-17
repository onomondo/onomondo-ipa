/*
 * Copyrighct (c) 2025 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, section 3.0.2: Common Cancel Session Procedure
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
#include "es10b_cancel_session.h"
#include "esipa_cancel_session.h"
#include "proc_cmn_cancel_sess.h"

/*! Perform Common Cancel Session Procedure.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] pars pointer to struct that holds the procedure parameters.
 *  \returns 0 on success, negative on failure. */
int ipa_proc_cmn_cancel_sess(struct ipa_context *ctx, const struct ipa_proc_cmn_cancel_sess_pars *pars)
{
	struct ipa_es10b_cancel_session_req es10b_cancel_session_req = { 0 };
	struct ipa_es10b_cancel_session_res *es10b_cancel_session_res = NULL;
	struct ipa_esipa_cancel_session_req esipa_cancel_session_req = { 0 };
	struct ipa_esipa_cancel_session_res *esipa_cancel_session_res = NULL;
	int rc;

	/* Cancel session on the eUICC side */
	es10b_cancel_session_req.req.transactionId = pars->transaction_id;
	es10b_cancel_session_req.req.reason = pars->reason;
	es10b_cancel_session_res = ipa_es10b_cancel_session(ctx, &es10b_cancel_session_req);
	if (!es10b_cancel_session_res) {
		rc = -EINVAL;
		goto error;
	}

	/* Cancel session on the eIM side */
	esipa_cancel_session_req.transaction_id = &es10b_cancel_session_req.req.transactionId;
	if (es10b_cancel_session_res->cancel_session_err)
		esipa_cancel_session_req.cancel_session_err = es10b_cancel_session_res->cancel_session_err;
	else
		esipa_cancel_session_req.cancel_session_ok = es10b_cancel_session_res->cancel_session_ok;
	esipa_cancel_session_res = ipa_esipa_cancel_session(ctx, &esipa_cancel_session_req);
	if (!esipa_cancel_session_res) {
		rc = -EINVAL;
		goto error;
	} else if (esipa_cancel_session_res->cancel_session_err) {
		rc = esipa_cancel_session_res->cancel_session_err;
		goto error;
	} else if (!esipa_cancel_session_res->cancel_session_ok) {
		rc = -EINVAL;
		goto error;
	}

	rc = 0;
error:
	ipa_es10b_cancel_session_res_free(es10b_cancel_session_res);
	ipa_esipa_cancel_session_res_free(esipa_cancel_session_res);
	return rc;
}
