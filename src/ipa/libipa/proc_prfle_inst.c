/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, section 3.1.3.3: Sub-procedure Profile Installation
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/log.h>
#include <StateChangeCause.h>
#include "context.h"
#include "utils.h"
#include "es10x.h"
#include "es10b_load_bnd_prfle_pkg.h"
#include "esipa_handle_notif.h"
#include "es10b_rm_notif_from_lst.h"
#include "es10b_enable_using_dd.h"
#include "bpp_segments.h"
#include "proc_prfle_inst.h"

/* Return codes: < 0 = error, 0 = ok, 1 = Result was present, notification sent */
static int handle_load_bnd_prfle_pkg_res(struct ipa_context *ctx, struct ipa_es10b_load_bnd_prfle_pkg_res *res,
					 long *seq_number, struct ProfileInstallationResult **pir)
{
	struct ipa_esipa_handle_notif_req handle_notif_req = { 0 };
	int rc;

	*seq_number = 0;

	/* No response is present, this is the normal case while a sequence of LoadBoundProfilePackage functions is
	 * executed. */
	if (!res->res) {
		ipa_es10b_load_bnd_prfle_res_free(res);
		return 0;
	}

	/* Instruct the eUICC to enable the newly installed profile (if configured and granted). When the
	 * profile actually got enabled, report the state change to the eIM on the next poll. */
	if (ipa_es10b_enable_using_dd(ctx) == 0)
		ctx->nvstate.pending_state_change_cause = StateChangeCause_immediateEnableProfile;

	/* A response is present, this is either the normal ending of the installation sequence or the eUICC has aborted
	 * the installation. In both situations we forward the ProfileInstallationResult to the eIM. */
	handle_notif_req.profile_installation_result = res->res;
	rc = ipa_esipa_handle_notif(ctx, &handle_notif_req);
	*seq_number = res->res->profileInstallationResultData.notificationMetadata.seqNumber;
	if (pir) {
		/* Hand the ProfileInstallationResult over to the caller (see header file) */
		*pir = res->res;
		res->res = NULL;
	}
	ipa_es10b_load_bnd_prfle_res_free(res);
	if (rc < 0)
		return -EINVAL;
	return 1;
}

/*! Perform Sub-procedure Profile Installation.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] pars pointer to struct that holds the procedure parameters.
 *  \returns 0 on success, negative on failure. */
int ipa_proc_prfle_inst(struct ipa_context *ctx, const struct ipa_proc_prfle_inst_pars *pars)
{
	struct ipa_es10b_load_bnd_prfle_pkg_res *load_bnd_prfle_pkg_res = NULL;
	struct ipa_bpp_segment_iter seg_iter;
	struct ipa_buf *segment = NULL;
	unsigned int i;
	int rc;
	long seq_number = -1;
	bool sucess = true;

	/* Step #3-#5 Split BPP into ES8+ segments and send the segments to eUICC. The segments are encoded one at a
	 * time and freed right after the eUICC has consumed them, so that only a single segment coexists with the
	 * decoded BoundProfilePackage. */
	ipa_bpp_segment_iter_init(&seg_iter, pars->bound_profile_package);
	for (i = 0; i < seg_iter.count; i++) {
		IPA_LOGP(SIPA, LDEBUG, "transferring ES8+ segments...\n");
		segment = ipa_bpp_segment_iter_next(&seg_iter);
		if (!segment) {
			IPA_LOGP(SIPA, LERROR, "failed to encode ES8+ segment!\n");
			goto error;
		}
		load_bnd_prfle_pkg_res = ipa_es10b_load_bnd_prfle_pkg(ctx, segment->data, segment->len);
		IPA_FREE(segment);
		if (!load_bnd_prfle_pkg_res) {
			IPA_LOGP(SIPA, LERROR, "failed to transfer ES8+ segments!\n");
			goto error;
		}
		rc = handle_load_bnd_prfle_pkg_res(ctx, load_bnd_prfle_pkg_res, &seq_number, pars->pir);
		if (rc < 0) {
			goto error;
		} else if (rc == 0 && i == seg_iter.count - 1) {
			IPA_LOGP(SIPA, LERROR, "eUICC didn't respond with ProfileInstallationResult!\n");
			goto error;
		} else if (rc == 1 && i != seg_iter.count - 1) {
			IPA_LOGP(SIPA, LERROR, "profile installation aborted by eUICC, notfication sent!\n");
			sucess = false;
			break;
		}
	}

	/* Step #11 (ES10b.RemoveNotificationFromList) */
	if (seq_number >= 0) {
		if (ipa_es10b_rm_notif_from_lst(ctx, seq_number) < 0)
			goto error;
	} else {
		/* We do not have a valid sequence number, either the loop above did not run at least once (0 segments)
		 * or something else went wrong. */
		goto error;
	}

	if (sucess)
		IPA_LOGP(SIPA, LINFO, "profile installation succeeded!\n");
	else
		IPA_LOGP(SIPA, LINFO, "profile installation aborted!\n");
	return 0;
error:
	IPA_LOGP(SIPA, LERROR, "profile installation failed!\n");
	return -EINVAL;
}
