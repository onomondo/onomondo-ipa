/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, 5.7.3: Function (ES10a): GetEuiccConfiguredAddresses
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <EuiccConfiguredAddressesRequest.h>
#include "context.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10a_get_euicc_cfg_addr.h"

static int dec_get_euicc_cfg_addr(struct ipa_es10a_euicc_cfg_addr *euicc_cfg_addr, const struct ipa_buf *es10a_res)
{
	struct EuiccConfiguredAddressesResponse *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_EuiccConfiguredAddressesResponse, es10a_res, "GetEuiccConfiguredAddresses");
	if (!asn)
		return -EINVAL;

	euicc_cfg_addr->res = asn;
	return 0;
}

/*! Function (ES10a): GetEuiccConfiguredAddresses.
 *  \param[inout] ctx pointer to ipa_context.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10a_euicc_cfg_addr *ipa_es10a_get_euicc_cfg_addr(struct ipa_context *ctx)
{
	struct ipa_buf *es10a_req = NULL;
	struct ipa_buf *es10a_res = NULL;
	struct ipa_es10a_euicc_cfg_addr *euicc_cfg_addr = IPA_ALLOC_ZERO(struct ipa_es10a_euicc_cfg_addr);
	struct EuiccConfiguredAddressesRequest get_euicc_cfg_addr_req = { 0 };
	int rc;

	es10a_req =
	    ipa_es10x_req_enc(&asn_DEF_EuiccConfiguredAddressesRequest, &get_euicc_cfg_addr_req,
			      "GetEuiccConfiguredAddresses");
	if (!es10a_req) {
		IPA_LOGP_ES10X("GetEuiccConfiguredAddresses", LERROR, "unable to encode ES10a request\n");
		goto error;
	}

	es10a_res = ipa_euicc_transceive_es10x(ctx, es10a_req);
	if (!es10a_res) {
		IPA_LOGP_ES10X("GetEuiccConfiguredAddresses", LERROR, "no ES10a response\n");
		goto error;
	}

	rc = dec_get_euicc_cfg_addr(euicc_cfg_addr, es10a_res);
	if (rc < 0)
		goto error;

	IPA_FREE(es10a_req);
	IPA_FREE(es10a_res);
	return euicc_cfg_addr;
error:
	IPA_FREE(es10a_req);
	IPA_FREE(es10a_res);
	ipa_es10a_get_euicc_cfg_addr_free(euicc_cfg_addr);
	return NULL;
}

/*! Free results of function (ES10a): GetEuiccConfiguredAddresses.
 *  \param[in] res pointer to function result. */
void ipa_es10a_get_euicc_cfg_addr_free(struct ipa_es10a_euicc_cfg_addr *res)
{
	IPA_ES10X_RES_FREE(asn_DEF_EuiccConfiguredAddressesResponse, res);
}
