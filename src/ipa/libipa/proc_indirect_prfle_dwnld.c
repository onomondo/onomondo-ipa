/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.32, section 3.2.3.2: Indirect Profile Download
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/log.h>
#include <ProfileDownloadTriggerResult.h>
#include "context.h"
#include "utils.h"
#include "activation_code.h"
#include "esipa_auth_clnt.h"
#include "proc_cmn_mtl_auth.h"
#include "proc_prfle_dwnld.h"
#include "esipa_get_bnd_prfle_pkg.h"
#include "proc_cmn_cancel_sess.h"
#include "proc_prfle_inst.h"
#include "esipa_prvde_eim_pkg_rslt.h"
#include "proc_indirect_prfle_dwnld.h"

/* Report the outcome of the triggered download back to the eIM (SGP.32 v1.2, section 2.11.2.3).
 * On success the ProfileInstallationResult is included, on failure a profileDownloadError. The
 * eimTransactionId from the trigger is echoed so the eIM can correlate the result. */
static void send_dwnld_trigger_result(struct ipa_context *ctx,
				      const struct ipa_proc_indirect_prfle_dwnlod_pars *pars,
				      const struct ProfileInstallationResult *pir)
{
	struct ProfileDownloadTriggerResult trigger_result = { 0 };
	struct ipa_esipa_prvde_eim_pkg_rslt_req prvde_eim_pkg_rslt_req = { 0 };
	struct ipa_esipa_prvde_eim_pkg_rslt_res *prvde_eim_pkg_rslt_res = NULL;

	trigger_result.eimTransactionId = (TransactionId_t *)pars->eim_transaction_id;
	if (pir) {
		trigger_result.profileDownloadTriggerResultData.present =
		    ProfileDownloadTriggerResult__profileDownloadTriggerResultData_PR_profileInstallationResult;
		trigger_result.profileDownloadTriggerResultData.choice.profileInstallationResult.
		    profileInstallationResultData = pir->profileInstallationResultData;
		trigger_result.profileDownloadTriggerResultData.choice.profileInstallationResult.euiccSignPIR =
		    pir->euiccSignPIR;
	} else {
		trigger_result.profileDownloadTriggerResultData.present =
		    ProfileDownloadTriggerResult__profileDownloadTriggerResultData_PR_profileDownloadError;
		trigger_result.profileDownloadTriggerResultData.choice.profileDownloadError.profileDownloadErrorReason =
		    ProfileDownloadTriggerResult__profileDownloadTriggerResultData__profileDownloadError__profileDownloadErrorReason_undefinedError;
	}

	prvde_eim_pkg_rslt_req.prfle_dwnld_trig_req_rslt = &trigger_result;
	prvde_eim_pkg_rslt_res = ipa_esipa_prvde_eim_pkg_rslt(ctx, &prvde_eim_pkg_rslt_req);
	if (!prvde_eim_pkg_rslt_res)
		IPA_LOGP(SIPA, LERROR, "unable to report the profile download result to the eIM!\n");

	/* Any eimAcknowledgements in the response are intentionally not processed here: the
	 * ProfileInstallationResult notification was already delivered to the eIM and removed
	 * from the eUICC by the profile installation sub-procedure. */
	ipa_esipa_prvde_eim_pkg_rslt_free(prvde_eim_pkg_rslt_res);
}

/*! Perform Indirect Profile Download Procedure.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] pars pointer to struct that holds the procedure parameters.
 *  \returns 0 on success, negative on failure. */
int ipa_proc_indirect_prfle_dwnlod(struct ipa_context *ctx, const struct ipa_proc_indirect_prfle_dwnlod_pars *pars)
{
	struct ipa_activation_code *activation_code = NULL;
	struct ipa_esipa_auth_clnt_res *auth_clnt_res = NULL;
	struct ipa_esipa_get_bnd_prfle_pkg_res *get_bnd_prfle_pkg_res = NULL;
	struct ipa_proc_cmn_mtl_auth_pars cmn_mtl_auth_pars = { 0 };
	struct ipa_proc_cmn_cancel_sess_pars cmn_cancel_sess_pars = { 0 };
	struct ipa_proc_prfle_dwnlod_pars prfle_dwnlod_pars = { 0 };
	struct ipa_proc_prfle_inst_pars prfle_inst_pars = { 0 };
	struct ProfileInstallationResult *pir = NULL;
	bool installed = false;

	/* This procedure is called when the IPAd receives an eIM package with a download trigger request
	 * (which contains the activation code) */

	activation_code = ipa_activation_code_parse(pars->ac);
	ipa_activation_code_dump(activation_code, 0, SIPA, LDEBUG);
	if (!activation_code) {
		IPA_LOGP(SIPA, LERROR, "cannot continue, activation code invalid or missing!\n");
		goto error;
	}

	/* Execute sub procedure: Common Mutual Authentication Procedure */
	cmn_mtl_auth_pars.tac = pars->tac;
	cmn_mtl_auth_pars.allowed_ca = pars->allowed_ca;
	cmn_mtl_auth_pars.smdp_addr = activation_code->sm_dp_plus_address;
	cmn_mtl_auth_pars.ac_token = activation_code->ac_token;
	auth_clnt_res = ipa_proc_cmn_mtl_auth(ctx, &cmn_mtl_auth_pars);
	if (!auth_clnt_res) {
		IPA_LOGP(SIPA, LERROR, "cannot continue, mutual authentication failed!\n");
		goto error;
	}

	/* TODO: Check if ProfileMetadata contains Profile Policy Rulses (PPRs) and apply the PPRs as configured on the
	 * eUICC. (This is an optional feature, which we currently do not support, see also proc_euicc_data_req.c) */

	/* TODO: remove this part as it is not required (see also github issue #5) */
	/* Execute sub procedure: Sub-procedure Profile Download and Installation – Download Confirmation */
	prfle_dwnlod_pars.auth_clnt_ok_dpe = auth_clnt_res->auth_clnt_ok_dpe;
	get_bnd_prfle_pkg_res = ipa_proc_prfle_dwnlod(ctx, &prfle_dwnlod_pars);
	if (!get_bnd_prfle_pkg_res) {
		IPA_LOGP(SIPA, LERROR, "sub procedure profile download has failed -- canceling session!\n");
		cmn_cancel_sess_pars.reason = CancelSessionReason_loadBppExecutionError;
		cmn_cancel_sess_pars.transaction_id = *auth_clnt_res->transaction_id;
		ipa_proc_cmn_cancel_sess(ctx, &cmn_cancel_sess_pars);
		goto error;
	}

	/* At this point we must ask the user for consent before we proceed with the profile installation. In case the
	 * user does not consent, we must abort by calling the common cancel session procedure. */
	if (ctx->cfg->prfle_inst_consent_cb
	    && !ctx->cfg->prfle_inst_consent_cb(activation_code->sm_dp_plus_address, activation_code->ac_token)) {
		IPA_LOGP(SIPA, LERROR, "no end user consent for profile installation -- canceling session!\n");
		cmn_cancel_sess_pars.reason = CancelSessionReason_endUserRejection;
		cmn_cancel_sess_pars.transaction_id = *auth_clnt_res->transaction_id;
		ipa_proc_cmn_cancel_sess(ctx, &cmn_cancel_sess_pars);
		goto error;
	}

	/* Execute sub procedure: Sub-procedure Profile Installation (See also section 3.1.3.3 of SGP.22) */
	prfle_inst_pars.bound_profile_package = &get_bnd_prfle_pkg_res->get_bnd_prfle_pkg_ok->boundProfilePackage;
	prfle_inst_pars.pir = &pir;
	if (ipa_proc_prfle_inst(ctx, &prfle_inst_pars) < 0) {
		IPA_LOGP(SIPA, LERROR, "sub procedure profile installation has failed -- canceling session!\n");
		cmn_cancel_sess_pars.reason = CancelSessionReason_loadBppExecutionError;
		cmn_cancel_sess_pars.transaction_id = *auth_clnt_res->transaction_id;
		ipa_proc_cmn_cancel_sess(ctx, &cmn_cancel_sess_pars);
	} else {
		installed = true;
	}

error:
	/* Report the outcome of the triggered download to the eIM */
	send_dwnld_trigger_result(ctx, pars, installed ? pir : NULL);

	ipa_activation_code_free(activation_code);
	ipa_esipa_auth_clnt_res_free(auth_clnt_res);
	ipa_esipa_get_bnd_prfle_pkg_res_free(get_bnd_prfle_pkg_res);
	if (pir)
		ASN_STRUCT_FREE(asn_DEF_ProfileInstallationResult, pir);
	return 0;
}
