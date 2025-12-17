/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 * 
 * See also: GSMA SGP.22, 5.7.20: Function (ES10c): GetEID
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
#include "GetEuiccDataRequest.h"
#include "GetEuiccDataResponse.h"
#include "es10c_get_eid.h"

static int dec_get_euicc_data_res(uint8_t *eid, const struct ipa_buf *es10b_res)
{
	struct GetEuiccDataResponse *asn = NULL;
	uint8_t eid_buf[IPA_LEN_EID];
	int rc = 0;

	asn = ipa_es10x_res_dec(&asn_DEF_GetEuiccDataResponse, es10b_res, "GetEID");
	if (!asn)
		return -EINVAL;

	if (asn->eidValue.size != IPA_LEN_EID) {
		IPA_LOGP_ES10X("GetEID", LERROR, "eID has wrong length, expected %u, got %zu\n", IPA_LEN_EID,
			       asn->eidValue.size);
		rc = -EINVAL;
		goto error;
	}

	IPA_COPY_ASN_BUF(eid_buf, &asn->eidValue);
	memcpy(eid, eid_buf, sizeof(eid_buf));

error:
	ASN_STRUCT_FREE(asn_DEF_GetEuiccDataResponse, asn);

	return rc;
}

/*! Function (ES10c): GetEID.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[out] eid eID of the eUICC.
 *  \returns 0 on success, negative on error. */
int ipa_es10c_get_eid(struct ipa_context *ctx, uint8_t *eid)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct GetEuiccDataRequest get_euicc_data_req = { 0 };
	int rc = -EINVAL;

	get_euicc_data_req.tagList.buf = (uint8_t *) "\x5A";
	get_euicc_data_req.tagList.size = 1;
	es10b_req = ipa_es10x_req_enc(&asn_DEF_GetEuiccDataRequest, &get_euicc_data_req, "GetEID");
	if (!es10b_req) {
		IPA_LOGP_ES10X("GetEID", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("GetEID", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_get_euicc_data_res(eid, es10b_res);
	if (rc < 0)
		goto error;

error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return rc;
}
