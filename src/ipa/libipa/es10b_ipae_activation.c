/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * See also: GSMA SGP.32 v1.2, 3.8.4: IPAe Activation
 *
 * IpaeActivation is a Device to ISD-R command whose only purpose is to activate an IPA inside the
 * eUICC (IPAe). A deployment that uses this library as its IPAd normally never sends it: a Device
 * whose Terminal Capability announces IPAd support keeps the IPAe deactivated simply by not sending
 * the request. The function is provided for completeness (e.g. Devices that hand over to an IPAe).
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <IpaeActivationRequest.h>
#include <IpaeActivationResponse.h>
#include "context.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_ipae_activation.h"

static const struct num_str_map error_code_strings[] = {
	{ IpaeActivationResponse__ipaeActivationResult_ok, "ok" },
	{ IpaeActivationResponse__ipaeActivationResult_notSupported, "notSupported" },
	{ 0, NULL }
};

static int dec_ipae_activation_res(const struct ipa_buf *es10b_res)
{
	struct IpaeActivationResponse *asn = NULL;
	int rc;

	asn = ipa_es10x_res_dec(&asn_DEF_IpaeActivationResponse, es10b_res, "IpaeActivation");
	if (!asn)
		return -EINVAL;

	rc = asn->ipaeActivationResult;

	if (rc == IpaeActivationResponse__ipaeActivationResult_ok) {
		IPA_LOGP_ES10X("IpaeActivation", LERROR, "function succeeded with status code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("IpaeActivation", LERROR, "function failed with error code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	}

	ASN_STRUCT_FREE(asn_DEF_IpaeActivationResponse, asn);
	return rc;
}

int ipae_activation(struct ipa_context *ctx, bool activate_ipae)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct IpaeActivationRequest ipae_activation_req = { 0 };
	int rc = -EINVAL;
	uint8_t ipae_opt[1] = { 0 };

	if (activate_ipae)
		ipae_opt[0] |= (1 << (7 - IpaeActivationRequest__ipaeOption_activateIpae));

	ipae_activation_req.ipaeOption.buf = ipae_opt;
	ipae_activation_req.ipaeOption.size = 1;
	ipae_activation_req.ipaeOption.bits_unused = 7;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_IpaeActivationRequest, &ipae_activation_req, "IpaeActivation");
	if (!es10b_req) {
		IPA_LOGP_ES10X("IpaeActivation", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("IpaeActivation", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_ipae_activation_res(es10b_res);
	if (rc < 0)
		goto error;

error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return rc;
}

int ipae_activation_emu(struct ipa_context *ctx)
{
	(void)ctx;

	/* A consumer eUICC has no IPAe to activate */
	IPA_LOGP_ES10X("IpaeActivation", LERROR,
		       "IoT eUICC emulation active, function failed with error code %d=%s!\n",
		       (int)IpaeActivationResponse__ipaeActivationResult_notSupported,
		       ipa_str_from_num(error_code_strings, IpaeActivationResponse__ipaeActivationResult_notSupported,
					"(unknown)"));
	return IpaeActivationResponse__ipaeActivationResult_notSupported;
}

/*! Function (ES10b): IpaeActivation.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] activate_ipae set the activateIpae option bit.
 *  \returns positive status code on success, negative on error. */
int ipa_es10b_ipae_activation(struct ipa_context *ctx, bool activate_ipae)
{
	if (ctx->cfg->iot_euicc_emu_enabled)
		return ipae_activation_emu(ctx);
	else
		return ipae_activation(ctx, activate_ipae);
}
