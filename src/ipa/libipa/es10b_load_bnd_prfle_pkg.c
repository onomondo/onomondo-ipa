/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, 5.7.6: Function (ES10b): LoadBoundProfilePackage
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
#include "length.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_load_bnd_prfle_pkg.h"

static void collect_auto_enable_data_from_prfle_inst_rslt(struct ipa_context *ctx,
							  struct ProfileInstallationResult *prfle_inst_rslt)
{
	struct ProfileInstallationResultData *prfle_inst_res_data = &prfle_inst_rslt->profileInstallationResultData;

	/* When the profile was not successfully installed, then there is no point in collecting any auto enable data. */
	if (prfle_inst_res_data->finalResult.present != ProfileInstallationResultData__finalResult_PR_successResult)
		return;

	/* Collect smdpOid and smdpAddress so that we can verify later whether the auto enable is granted or not. */
	ctx->iot_euicc_emu.auto_enable.smdp_oid = IPA_BUF_FROM_ASN(&prfle_inst_res_data->smdpOid);
	ctx->iot_euicc_emu.auto_enable.smdp_address =
	    IPA_BUF_FROM_ASN(&prfle_inst_res_data->notificationMetadata.notificationAddress);

	/* Collect AID of the profile so that we later know which profile to enable. */
	ctx->iot_euicc_emu.auto_enable.profile_aid =
	    IPA_BUF_FROM_ASN(&prfle_inst_res_data->finalResult.choice.successResult.aid);
}

static int dec_prfle_inst_res(struct ipa_es10b_load_bnd_prfle_pkg_res *res, const struct ipa_buf *es10b_res)
{
	struct ProfileInstallationResult *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_ProfileInstallationResult, es10b_res, "LoadBoundProfilePackage");
	if (!asn)
		return -EINVAL;

	res->res = asn;
	return 0;
}

/*! Function (ES10b): LoadBoundProfilePackage.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] segment pointer to (encrypted) ES8+ TLV segment.
 *  \param[in] segment_len length of segment data.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10b_load_bnd_prfle_pkg_res *ipa_es10b_load_bnd_prfle_pkg(struct ipa_context *ctx, const uint8_t *segment,
								      size_t segment_len)
{
	struct ipa_buf es10b_req;
	struct ipa_buf *es10b_res = NULL;
	struct ipa_es10b_load_bnd_prfle_pkg_res *res = IPA_ALLOC_ZERO(struct ipa_es10b_load_bnd_prfle_pkg_res);
	int rc;

	/* In case IoT eUICC emulation is active, ensure that the auto enable data is cleared. (This data has no
	 * relevance in case a real IoT eUICC is used.) */
	ipa_buf_free(ctx->iot_euicc_emu.auto_enable.smdp_oid);
	ctx->iot_euicc_emu.auto_enable.smdp_oid = NULL;
	ipa_buf_free(ctx->iot_euicc_emu.auto_enable.smdp_address);
	ctx->iot_euicc_emu.auto_enable.smdp_address = NULL;
	ipa_buf_free(ctx->iot_euicc_emu.auto_enable.profile_aid);
	ctx->iot_euicc_emu.auto_enable.profile_aid = NULL;

	/* The ES10b LoadBoundProfilePackage function is a pseudo function that only exists as a placeholder in the
	 * specification. In practice it is just a STORE DATA command that carries a profile segment without any
	 * envelope around. (See also SGP.22, section 5.7.6 and SGP.22, section 2.5.5 */

	ipa_buf_assign(&es10b_req, segment, segment_len);
	es10b_res = ipa_euicc_transceive_es10x(ctx, &es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("LoadBoundProfilePackage", LERROR, "no ES10b response\n");
		goto error;
	}

	/* The ES10b LoadBoundProfilePackage function only returns a response on the final function call of a sequence
	 * This response contains the ProfileInstallationResult. */
	if (es10b_res->len) {
		rc = dec_prfle_inst_res(res, es10b_res);
		if (rc < 0)
			goto error;

		/* In case IoT eUICC emulation is active, collect auto enable data to support the emulation of
		 * the ES10b function EnableUsingDD. */
		if (ctx->cfg->iot_euicc_emu_enabled)
			collect_auto_enable_data_from_prfle_inst_rslt(ctx, res->res);
	}

	IPA_FREE(es10b_res);
	return res;
error:
	IPA_FREE(es10b_res);
	IPA_FREE(res);
	return NULL;
}

/*! Free results of function (ES10b): LoadBoundProfilePackage.
 *  \param[in] res pointer to function result. */
void ipa_es10b_load_bnd_prfle_res_free(struct ipa_es10b_load_bnd_prfle_pkg_res *res)
{
	IPA_ES10X_RES_FREE(asn_DEF_ProfileInstallationResult, res);
}
