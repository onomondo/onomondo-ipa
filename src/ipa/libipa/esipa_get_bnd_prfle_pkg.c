/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 * 
 * See also: GSMA SGP.32, section 5.14.2: Function: (ESipa) GetBoundProfilePackage
 */

#include <stdint.h>
#include <errno.h>
#include <onomondo/ipa/http.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/ipad.h>
#include <EsipaMessageFromIpaToEim.h>
#include <SGP32-PrepareDownloadResponse.h>
#include <GetBoundProfilePackageRequestEsipa.h>
#include <GetBoundProfilePackageResponseEsipa.h>
#include <onomondo/ipa/utils.h>
#include "utils.h"
#include "length.h"
#include "context.h"
#include "esipa.h"
#include "esipa_get_bnd_prfle_pkg.h"

static const struct num_str_map error_code_strings[] = {
	{ GetBoundProfilePackageResponseEsipa__getBoundProfilePackageErrorEsipa_euiccSignatureInvalid,
	 "euiccSignatureInvalid" },
	{ GetBoundProfilePackageResponseEsipa__getBoundProfilePackageErrorEsipa_confirmationCodeMissing,
	 "confirmationCodeMissing" },
	{ GetBoundProfilePackageResponseEsipa__getBoundProfilePackageErrorEsipa_confirmationCodeRefused,
	 "confirmationCodeRefused" },
	{ GetBoundProfilePackageResponseEsipa__getBoundProfilePackageErrorEsipa_confirmationCodeRetriesExceeded,
	 "confirmationCodeRetriesExceeded" },
	{ GetBoundProfilePackageResponseEsipa__getBoundProfilePackageErrorEsipa_bppRebindingRefused,
	 "bppRebindingRefused" },
	{ GetBoundProfilePackageResponseEsipa__getBoundProfilePackageErrorEsipa_downloadOrderExpired,
	 "downloadOrderExpired" },
	{ GetBoundProfilePackageResponseEsipa__getBoundProfilePackageErrorEsipa_metadataMismatch,
	 "profileMetadataMismatch" },
	{ GetBoundProfilePackageResponseEsipa__getBoundProfilePackageErrorEsipa_invalidTransactionId,
	 "invalidTransactionId" },
	{ GetBoundProfilePackageResponseEsipa__getBoundProfilePackageErrorEsipa_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static struct ipa_buf *enc_get_bnd_prfle_pkg_req(const struct ipa_esipa_get_bnd_prfle_pkg_req *req)
{
	struct EsipaMessageFromIpaToEim msg_to_eim = { 0 };

	msg_to_eim.present = EsipaMessageFromIpaToEim_PR_getBoundProfilePackageRequestEsipa;

	switch (req->prep_dwnld_res->present) {
	case PrepareDownloadResponse_PR_downloadResponseOk:
		msg_to_eim.choice.getBoundProfilePackageRequestEsipa.prepareDownloadResponse.present =
		    SGP32_PrepareDownloadResponse_PR_downloadResponseOk;
		msg_to_eim.choice.getBoundProfilePackageRequestEsipa.prepareDownloadResponse.choice.downloadResponseOk =
		    req->prep_dwnld_res->choice.downloadResponseOk;
		msg_to_eim.choice.getBoundProfilePackageRequestEsipa.transactionId =
		    req->prep_dwnld_res->choice.downloadResponseOk.euiccSigned2.transactionId;
		break;
	case PrepareDownloadResponse_PR_downloadResponseError:
		msg_to_eim.choice.getBoundProfilePackageRequestEsipa.prepareDownloadResponse.present =
		    SGP32_PrepareDownloadResponse_PR_downloadResponseError;
		msg_to_eim.choice.getBoundProfilePackageRequestEsipa.prepareDownloadResponse.choice.
		    downloadResponseError = req->prep_dwnld_res->choice.downloadResponseError;
		msg_to_eim.choice.getBoundProfilePackageRequestEsipa.transactionId =
		    req->prep_dwnld_res->choice.downloadResponseError.transactionId;
		break;
	default:
		IPA_LOGP_ESIPA("GetBoundProfilePackage", LINFO,
			       "prepare download response is empty, cannot encode request\n");
		return NULL;
	}

	/* Encode */
	return ipa_esipa_msg_to_eim_enc(&msg_to_eim, "GetBoundProfilePackage");
}

/* Wire tags on the GetBoundProfilePackage response path */
#define TAG_GET_BPP_RESPONSE 0xBF3A /* EsipaMessageFromEimToIpa [58] */
#define TAG_OK_SEQUENCE      0x30   /* GetBoundProfilePackageOkEsipa */
#define TAG_ERROR_INTEGER    0x02   /* getBoundProfilePackageErrorEsipa */
#define TAG_BPP              0xBF36 /* boundProfilePackage [54] */

/* Locate the BoundProfilePackage inside the raw ESipa response instead of
 * BER-decoding the whole message into an asn1c tree: only the BPP bytes are
 * ever used, and the tree cost ~2x the wire size right at the heap peak.
 * Takes ownership of esipa_res (it backs the returned slice). */
static struct ipa_esipa_get_bnd_prfle_pkg_res *dec_get_bnd_prfle_pkg_res(struct ipa_buf *esipa_res)
{
	struct ipa_esipa_get_bnd_prfle_pkg_res *res;
	const uint8_t *data = esipa_res->data;
	size_t remaining = esipa_res->len;
	uint16_t tag;
	size_t val_len, hdr_len, off = 0;

	res = IPA_ALLOC_ZERO(struct ipa_esipa_get_bnd_prfle_pkg_res);
	res->raw = esipa_res;

	/* EsipaMessageFromEimToIpa: expect the GetBoundProfilePackage response */
	hdr_len = ipa_tlv_hdr(data, remaining, &tag, &val_len);
	if (!hdr_len || tag != TAG_GET_BPP_RESPONSE) {
		IPA_LOGP_ESIPA("GetBoundProfilePackage", LERROR, "unexpected response content!\n");
		goto unexpected;
	}
	off = hdr_len;
	remaining = hdr_len + val_len;

	/* CHOICE: ok (SEQUENCE) or error code (INTEGER) */
	hdr_len = ipa_tlv_hdr(data + off, remaining - off, &tag, &val_len);
	if (!hdr_len) {
		IPA_LOGP_ESIPA("GetBoundProfilePackage", LERROR, "unexpected response content!\n");
		goto unexpected;
	}
	if (tag == TAG_ERROR_INTEGER) {
		long err = 0;
		size_t i;

		if (val_len == 0 || val_len > sizeof(err)) {
			IPA_LOGP_ESIPA("GetBoundProfilePackage", LERROR, "unexpected response content!\n");
			goto unexpected;
		}
		for (i = 0; i < val_len; i++)
			err = (err << 8) | data[off + hdr_len + i];
		res->get_bnd_prfle_pkg_err = err;
		IPA_LOGP_ESIPA("GetBoundProfilePackage", LERROR, "function failed with error code %ld=%s!\n",
			       err, ipa_str_from_num(error_code_strings, err, "(unknown)"));
		return res;
	}
	if (tag != TAG_OK_SEQUENCE) {
		IPA_LOGP_ESIPA("GetBoundProfilePackage", LERROR, "unexpected response content!\n");
		goto unexpected;
	}
	IPA_LOGP_ESIPA("GetBoundProfilePackage", LINFO,
		       "GetBoundProfilePackageResponseEsipa_PR_getBoundProfilePackageOkEsipa\n");

	/* GetBoundProfilePackageOkEsipa: [transactionId] boundProfilePackage */
	off += hdr_len;
	remaining = off + val_len;
	while (off < remaining) {
		hdr_len = ipa_tlv_hdr(data + off, remaining - off, &tag, &val_len);
		if (!hdr_len)
			break;
		if (tag == TAG_BPP) {
			res->bpp = data + off;
			res->bpp_len = hdr_len + val_len;
			return res;
		}
		off += hdr_len + val_len;
	}
	IPA_LOGP_ESIPA("GetBoundProfilePackage", LERROR, "response carries no BoundProfilePackage!\n");

unexpected:
	res->get_bnd_prfle_pkg_err = -1;
	return res;
}

/*! Function: (ESipa) GetBoundProfilePackage.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_esipa_get_bnd_prfle_pkg_res *ipa_esipa_get_bnd_prfle_pkg(struct ipa_context *ctx,
								    const struct ipa_esipa_get_bnd_prfle_pkg_req *req)
{
	struct ipa_buf *esipa_req = NULL;
	struct ipa_buf *esipa_res = NULL;

	IPA_LOGP_ESIPA("GetBoundProfilePackage", LINFO, "Preparing encoded profile package request\n");
	esipa_req = enc_get_bnd_prfle_pkg_req(req);
	if (!esipa_req)
		goto error;

	IPA_LOGP_ESIPA("GetBoundProfilePackage", LINFO, "Requesting profile package from eIM\n");
	esipa_res = ipa_esipa_req(ctx, esipa_req, "GetBoundProfilePackage");
	IPA_FREE(esipa_req);
	esipa_req = NULL;
	if (!esipa_res)
		goto error;

	IPA_LOGP_ESIPA("GetBoundProfilePackage", LINFO, "Locating profile package received from eIM\n");
	/* Ownership of esipa_res moves into the result: the BoundProfilePackage
	 * stays a slice of the wire bytes all the way into the ES8+ transfer. */
	return dec_get_bnd_prfle_pkg_res(esipa_res);

error:
	IPA_FREE(esipa_req);
	return NULL;
}

/*! Free results of function: (ESipa) GetBoundProfilePackage.
 *  \param[in] res pointer to function result. */
void ipa_esipa_get_bnd_prfle_pkg_res_free(struct ipa_esipa_get_bnd_prfle_pkg_res *res)
{
	if (!res)
		return;
	IPA_FREE(res->raw);
	IPA_FREE(res);
}
