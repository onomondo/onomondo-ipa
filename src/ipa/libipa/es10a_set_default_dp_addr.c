/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * See also: GSMA SGP.22, 5.7.4: Function (ES10a): SetDefaultDpAddress
 *
 * Only used by the IoT eUICC emulation: the SGP.32 setDefaultDpAddress PSMO is forwarded to the
 * consumer eUICC through this function, so the default SM-DP+ address genuinely lives on the card
 * (consistent with GetEuiccConfiguredAddresses and eUICCMemoryReset).
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <SetDefaultDpAddressRequest.h>
#include <SetDefaultDpAddressResponse.h>
#include "context.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10a_set_default_dp_addr.h"

static const struct num_str_map error_code_strings[] = {
	{ SetDefaultDpAddressResponse__setDefaultDpAddressResult_ok, "ok" },
	{ SetDefaultDpAddressResponse__setDefaultDpAddressResult_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_set_default_dp_addr_res(const struct ipa_buf *es10a_res)
{
	struct SetDefaultDpAddressResponse *asn = NULL;
	int rc;

	asn = ipa_es10x_res_dec(&asn_DEF_SetDefaultDpAddressResponse, es10a_res, "SetDefaultDpAddress");
	if (!asn)
		return -EINVAL;

	rc = asn->setDefaultDpAddressResult;

	if (rc == SetDefaultDpAddressResponse__setDefaultDpAddressResult_ok) {
		IPA_LOGP_ES10X("SetDefaultDpAddress", LERROR, "function succeeded with status code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	} else {
		IPA_LOGP_ES10X("SetDefaultDpAddress", LERROR, "function failed with error code %d=%s!\n",
			       rc, ipa_str_from_num(error_code_strings, rc, "(unknown)"));
	}

	ASN_STRUCT_FREE(asn_DEF_SetDefaultDpAddressResponse, asn);
	return rc;
}

/*! Function (ES10a): SetDefaultDpAddress.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] addr new default SM-DP+ address, an empty string removes the configured address.
 *  \returns positive status code on success, negative on error. */
int ipa_es10a_set_default_dp_addr(struct ipa_context *ctx, const UTF8String_t *addr)
{
	struct ipa_buf *es10a_req = NULL;
	struct ipa_buf *es10a_res = NULL;
	struct SetDefaultDpAddressRequest set_default_dp_addr_req = { 0 };
	int rc = -EINVAL;

	set_default_dp_addr_req.defaultDpAddress = *addr;

	es10a_req = ipa_es10x_req_enc(&asn_DEF_SetDefaultDpAddressRequest, &set_default_dp_addr_req,
				      "SetDefaultDpAddress");
	if (!es10a_req) {
		IPA_LOGP_ES10X("SetDefaultDpAddress", LERROR, "unable to encode ES10a request\n");
		goto error;
	}

	es10a_res = ipa_euicc_transceive_es10x(ctx, es10a_req);
	if (!es10a_res) {
		IPA_LOGP_ES10X("SetDefaultDpAddress", LERROR, "no ES10a response\n");
		goto error;
	}

	rc = dec_set_default_dp_addr_res(es10a_res);
	if (rc < 0)
		goto error;

error:
	IPA_FREE(es10a_req);
	IPA_FREE(es10a_res);
	return rc;
}
