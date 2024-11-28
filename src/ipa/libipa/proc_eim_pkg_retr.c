/*
 * Author: Philipp Maier
 * See also: GSMA SGP.32, section 3.1.1.1: eIM Package Retrieval
 *
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/ipad.h>
#include "context.h"
#include "utils.h"
#include "esipa.h"
#include "esipa_get_eim_pkg.h"
#include "es10b_get_eim_cfg_data.h"
#include "proc_cmn_mtl_auth.h"
#include "proc_cmn_cancel_sess.h"
#include "proc_indirect_prfle_dwnld.h"
#include "proc_euicc_pkg_dwnld_exec.h"
#include "proc_euicc_data_req.h"
#include "proc_eim_pkg_retr.h"

static int get_euicc_ci_pkid(struct ipa_context *ctx, struct ipa_buf **pkid)
{
	struct ipa_es10b_eim_cfg_data *eim_cfg_data = NULL;
	struct EimConfigurationData *eim_cfg_data_item = NULL;

	*pkid = NULL;

	eim_cfg_data = ipa_es10b_get_eim_cfg_data(ctx);
	if (!eim_cfg_data) {
		IPA_LOGP(SIPA, LERROR, "cannot read EimConfigurationData from eUICC\n");
		goto error;
	}

	eim_cfg_data_item = ipa_es10b_get_eim_cfg_data_filter(eim_cfg_data, ctx->eim_id);
	if (!eim_cfg_data_item) {
		IPA_LOGP(SIPA, LERROR, "no EimConfigurationData item for eimId %s present!\n", ctx->eim_id);
		goto error;
	}

	if (eim_cfg_data_item->euiccCiPKId)
		*pkid = IPA_BUF_FROM_ASN(eim_cfg_data_item->euiccCiPKId);

	ipa_es10b_get_eim_cfg_data_free(eim_cfg_data);
	return 0;
error:
	IPA_LOGP(SIPA, LERROR, "unable to retrieve EimConfigurationData\n");
	ipa_es10b_get_eim_cfg_data_free(eim_cfg_data);
	return -EINVAL;
}

/* Relay package contents to suitable handler procedure */
int eim_pkg_exec(struct ipa_context *ctx, const struct ipa_esipa_get_eim_pkg_res *get_eim_pkg_res)
{
	struct ipa_buf *allowed_ca_pkid = NULL;
	int rc;

	if (get_eim_pkg_res->euicc_package_request) {
		/* This must not happen. The internal logic in ipad_poll must make sure that
		 * ipa_proc_eucc_pkg_dwnld_exec_onset runs first in case ctx->proc_eucc_pkg_dwnld_exec_res is
		 * populated. */
		assert(!ctx->proc_eucc_pkg_dwnld_exec_res);

		ctx->proc_eucc_pkg_dwnld_exec_res =
		    ipa_proc_eucc_pkg_dwnld_exec(ctx, get_eim_pkg_res->euicc_package_request);
		if (!ctx->proc_eucc_pkg_dwnld_exec_res)
			goto error;

		/* In case the result of ipa_proc_eucc_pkg_dwnld_exec indicates that calling of
		 * ipa_proc_eucc_pkg_dwnld_exec_onset is not required, we throw away proc_eucc_pkg_dwnld_exec_res
		 * immediately. */
		if (ctx->proc_eucc_pkg_dwnld_exec_res && !ctx->proc_eucc_pkg_dwnld_exec_res->call_onset) {
			ipa_proc_eucc_pkg_dwnld_exec_res_free(ctx->proc_eucc_pkg_dwnld_exec_res);
			ctx->proc_eucc_pkg_dwnld_exec_res = NULL;
		}
	} else if (get_eim_pkg_res->ipa_euicc_data_request) {
		struct ipa_proc_euicc_data_req_pars euicc_data_req_pars = { 0 };
		euicc_data_req_pars.ipa_euicc_data_request = get_eim_pkg_res->ipa_euicc_data_request;
		rc = ipa_proc_euicc_data_req(ctx, &euicc_data_req_pars);
		if (rc < 0)
			goto error;
	} else if (get_eim_pkg_res->dwnld_trigger_request) {
		struct ipa_proc_indirect_prfle_dwnlod_pars indirect_prfle_dwnlod_pars = { 0 };
		if (!get_eim_pkg_res->dwnld_trigger_request->profileDownloadData) {
			/* In case the IPA capability eimDownloadDataHandling used, profileDownloadData would not be
			 * present. However, this is feature this IPAd implementation does not support. */
			IPA_LOGP(SIPA, LERROR,
				 "the ProfileDownloadTriggerRequest does not contain ProfileDownloadData -- cannot continue!\n");
			rc = -EINVAL;
			goto error;
		}
		if (get_eim_pkg_res->dwnld_trigger_request->profileDownloadData->present !=
		    ProfileDownloadData_PR_activationCode) {
			/* (see comment above) */
			IPA_LOGP(SIPA, LERROR,
				 "the ProfileDownloadData does not contain an activationCode -- cannot continue!\n");
			rc = -EINVAL;
			goto error;
		}

		rc = get_euicc_ci_pkid(ctx, &allowed_ca_pkid);
		if (rc < 0) {
			rc = -EINVAL;
			goto error;
		}

		indirect_prfle_dwnlod_pars.allowed_ca = allowed_ca_pkid;
		indirect_prfle_dwnlod_pars.tac = ctx->cfg->tac;
		indirect_prfle_dwnlod_pars.ac =
		    IPA_STR_FROM_ASN(&get_eim_pkg_res->dwnld_trigger_request->profileDownloadData->
				     choice.activationCode);
		rc = ipa_proc_indirect_prfle_dwnlod(ctx, &indirect_prfle_dwnlod_pars);
		IPA_FREE((void *)indirect_prfle_dwnlod_pars.ac);
		if (rc < 0)
			goto error;
	} else {
		IPA_LOGP(SIPA, LERROR,
			 "the GetEimPackageResponse contains an unsupported request -- cannot continue!\n");
		rc = -EINVAL;
		goto error;
	}

	IPA_FREE(allowed_ca_pkid);
	IPA_LOGP(SIPA, LINFO, "eIM Package Execution finished!\n");
	return 0;
error:
	IPA_FREE(allowed_ca_pkid);
	IPA_LOGP(SIPA, LERROR, "eIM Package Execution failed!\n");
	return rc;
}

/*! Perform eIM Package Retrieval Procedure.
 *  \param[inout] ctx pointer to ipa_context.
 *  \returns 0 on success, negative on failure. */
int ipa_proc_eim_pkg_retr(struct ipa_context *ctx)
{
	struct ipa_esipa_get_eim_pkg_res *get_eim_pkg_res = NULL;
	int rc;

	/* Ensure that we start with a fresh connection */
	ipa_esipa_close(ctx);

	/* Poll eIM */
	get_eim_pkg_res = ipa_esipa_get_eim_pkg(ctx, ctx->eid);
	if (!get_eim_pkg_res) {
		rc = -EINVAL;
		goto error;
	} else if (get_eim_pkg_res->eim_pkg_err == GetEimPackageResponse__eimPackageError_noEimPackageAvailable) {
		rc = -GetEimPackageResponse__eimPackageError_noEimPackageAvailable;
		goto error;
	} else if (get_eim_pkg_res->eim_pkg_err) {
		rc = -EINVAL;
		goto error;
	}

	IPA_LOGP(SIPA, LINFO, "eIM Package Retrieval succeeded!\n");
	rc = eim_pkg_exec(ctx, get_eim_pkg_res);
	ipa_esipa_get_eim_pkg_free(get_eim_pkg_res);
	ipa_esipa_close(ctx);
	return rc;
error:
	ipa_esipa_get_eim_pkg_free(get_eim_pkg_res);
	IPA_LOGP(SIPA, LINFO, "eIM Package Retrieval failed!\n");
	ipa_esipa_close(ctx);
	return rc;
}
