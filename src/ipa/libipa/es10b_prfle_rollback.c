/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.32, 5.9.16: Function (ES10b): ProfileRollback
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
#include "es10b_prfle_rollback.h"
#include "es10c_enable_prfle.h"
#include "proc_euicc_pkg_dwnld_exec.h"
#include "es10b_load_euicc_pkg.h"

static const struct num_str_map error_code_strings[] = {
	{ ProfileRollbackResponse__cmdResult_ok, "ok" },
	{ ProfileRollbackResponse__cmdResult_rollbackNotAllowed, "rollbackNotAllowed" },
	{ ProfileRollbackResponse__cmdResult_catBusy, "catBusy" },
	{ ProfileRollbackResponse__cmdResult_commandError, "commandError" },
	{ ProfileRollbackResponse__cmdResult_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_prfle_rollback_res(struct ipa_es10b_prfle_rollback_res *res, const struct ipa_buf *es10b_res)
{
	struct ProfileRollbackResponse *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_ProfileRollbackResponse, es10b_res, "ProfileRollback");
	if (!asn)
		return -EINVAL;

	if (asn->cmdResult != ProfileRollbackResponse__cmdResult_ok) {
		IPA_LOGP_ES10X("ProfileRollback", LERROR, "function failed with error code %ld=%s!\n",
			       asn->cmdResult, ipa_str_from_num(error_code_strings, asn->cmdResult, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("ProfileRollback", LERROR, "function succeeded with status code %ld=%s!\n",
			       asn->cmdResult, ipa_str_from_num(error_code_strings, asn->cmdResult, "(unknown)"));
	}

	res->res = asn;
	return 0;
}

static struct ipa_es10b_prfle_rollback_res *prfle_rollback(struct ipa_context *ctx, bool refresh_flag)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ipa_es10b_prfle_rollback_res *res = IPA_ALLOC_ZERO(struct ipa_es10b_prfle_rollback_res);
	struct ProfileRollbackRequest req = { 0 };
	int rc;

	req.refreshFlag = refresh_flag;
	es10b_req = ipa_es10x_req_enc(&asn_DEF_ProfileRollbackRequest, &req, "ProfileRollback");
	if (!es10b_req) {
		IPA_LOGP_ES10X("ProfileRollback", LERROR, "unable to encode Es10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("ProfileRollback", LERROR, "no Es10b response\n");
		goto error;
	}

	rc = dec_prfle_rollback_res(res, es10b_res);
	if (rc < 0)
		goto error;

	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return res;
error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	ipa_es10b_prfle_rollback_res_free(res);
	return NULL;
}

static void append_rollback_result(struct EuiccPackageResult *euicc_package_result, bool rollbacl_successful)
{
	struct EuiccResultData *euicc_result_data = IPA_ALLOC_ZERO(struct EuiccResultData);
	euicc_result_data->present = EuiccResultData_PR_rollbackResult;
	if (rollbacl_successful)
		euicc_result_data->choice.rollbackResult = RollbackProfileResult_ok;
	else
		euicc_result_data->choice.rollbackResult = RollbackProfileResult_undefinedError;
	ASN_SEQUENCE_ADD(&euicc_package_result->choice.euiccPackageResultSigned.euiccPackageResultDataSigned.
			 euiccResult.list, euicc_result_data);
}

static struct ipa_es10b_prfle_rollback_res *prfle_rollback_emu(struct ipa_context *ctx, bool refresh_flag)
{
	struct ipa_es10b_prfle_rollback_res *res = IPA_ALLOC_ZERO(struct ipa_es10b_prfle_rollback_res);
	struct ipa_es10c_enable_prfle_req enable_prfle_req = { 0 };
	struct ipa_es10c_enable_prfle_res *enable_prfle_res = NULL;

	IPA_LOGP_ES10X("ProfileRollback", LINFO,
		       "IoT eUICC emulation active, using ES10c function EnableProfile to rollback the profile ...\n");

	if (!ctx->iot_euicc_emu.rollback_iccid) {
		IPA_LOGP_ES10X("ProfileRollback", LERROR,
			       "cannot perform profile rollback, no rollback iccid known!\n");
		return NULL;
	}

	IPA_LOGP_ES10X("ProfileRollback", LINFO, "attempting to roll back to profile ICCD:%s...\n",
		       ipa_buf_hexdump(ctx->iot_euicc_emu.rollback_iccid));

	/* An IoT eUICC would store a rollback ICCID internally. Here we have to rely on the rollback_euicc that we
	 * have recorded before we attempted to select a different profile (via PSMO) */
	enable_prfle_req.req.profileIdentifier.present = EnableProfileRequest__profileIdentifier_PR_iccid;
	IPA_ASSIGN_IPA_BUF_TO_ASN(enable_prfle_req.req.profileIdentifier.choice.iccid, ctx->iot_euicc_emu.rollback_iccid);

	enable_prfle_req.req.refreshFlag = refresh_flag;
	enable_prfle_res = ipa_es10c_enable_prfle(ctx, &enable_prfle_req);
	if (enable_prfle_res) {
		res->res = IPA_ALLOC_ZERO(struct ProfileRollbackResponse);
		switch (enable_prfle_res->res->enableResult) {
		case EnableProfileResponse__enableResult_ok:
			res->res->cmdResult = ProfileRollbackResponse__cmdResult_ok;
			break;
		case EnableProfileResponse__enableResult_catBusy:
			res->res->cmdResult = ProfileRollbackResponse__cmdResult_catBusy;
			break;
		case EnableProfileResponse__enableResult_iccidOrAidNotFound:
		case EnableProfileResponse__enableResult_profileNotInDisabledState:
		case EnableProfileResponse__enableResult_disallowedByPolicy:
		case EnableProfileResponse__enableResult_wrongProfileReenabling:
		case EnableProfileResponse__enableResult_undefinedError:
		default:
			res->res->cmdResult = ProfileRollbackResponse__cmdResult_undefinedError;
		}

		if (ctx->proc_eucc_pkg_dwnld_exec_res && ctx->proc_eucc_pkg_dwnld_exec_res->load_euicc_pkg_res
		    && ctx->proc_eucc_pkg_dwnld_exec_res->load_euicc_pkg_res->res) {
			/* sneak into the cached results of the Generic eUICC Package Download and Execution procedure
			 * and use the EuiccPackageResult that was generated while executing the LoadEuiccPackage
			 * function. */
			res->res->eUICCPackageResult =
			    ipa_asn1c_dup(&asn_DEF_EuiccPackageResult,
					  ctx->proc_eucc_pkg_dwnld_exec_res->load_euicc_pkg_res->res);

			/* Append the result of this rollback maneuver */
			append_rollback_result(res->res->eUICCPackageResult,
					       res->res->cmdResult == ProfileRollbackResponse__cmdResult_ok);
		}
	} else {
		ipa_es10b_prfle_rollback_res_free(res);
		return NULL;
	}

	ipa_es10c_enable_prfle_res_free(enable_prfle_res);
	return res;
}

/*! Function (Es10b): ProfileRollback.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] refresh_flag (see SGP.22, section 3.2.1).
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10b_prfle_rollback_res *ipa_es10b_prfle_rollback(struct ipa_context *ctx, bool refresh_flag)
{
	if (ctx->cfg->iot_euicc_emu_enabled)
		return prfle_rollback_emu(ctx, refresh_flag);
	else
		return prfle_rollback(ctx, refresh_flag);
}

/*! Free results of function (Es10b): ProfileRollback.
 *  \param[in] res pointer to function result. */
void ipa_es10b_prfle_rollback_res_free(struct ipa_es10b_prfle_rollback_res *res)
{
	IPA_ES10X_RES_FREE(asn_DEF_ProfileRollbackResponse, res);
}
