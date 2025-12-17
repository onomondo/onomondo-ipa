/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.32, section 5.14.5: Function (ESipa): GetEimPackage
 */

#include <stdint.h>
#include <onomondo/ipa/http.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/ipad.h>
#include <EsipaMessageFromIpaToEim.h>
#include <GetEimPackageRequest.h>
#include "utils.h"
#include "length.h"
#include "context.h"
#include "esipa.h"
#include "esipa_get_eim_pkg.h"

static const struct num_str_map error_code_strings[] = {
	{ GetEimPackageResponse__eimPackageError_noEimPackageAvailable, "noEimPackageAvailable" },
	{ GetEimPackageResponse__eimPackageError_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static struct ipa_buf *enc_get_eim_pkg_req(const uint8_t *eid_value)
{
	struct EsipaMessageFromIpaToEim msg_to_eim = { 0 };

	msg_to_eim.present = EsipaMessageFromIpaToEim_PR_getEimPackageRequest;
	msg_to_eim.choice.getEimPackageRequest.eidValue.buf = (uint8_t *) eid_value;
	msg_to_eim.choice.getEimPackageRequest.eidValue.size = IPA_LEN_EID;

	return ipa_esipa_msg_to_eim_enc(&msg_to_eim, "GetEimPackage");
}

struct ipa_esipa_get_eim_pkg_res *dec_get_eim_pkg_req(const struct ipa_buf *msg_to_ipa_encoded)
{
	struct EsipaMessageFromEimToIpa *msg_to_ipa = NULL;
	struct ipa_esipa_get_eim_pkg_res *res = NULL;

	msg_to_ipa =
	    ipa_esipa_msg_to_ipa_dec(msg_to_ipa_encoded, "GetEimPackage",
				     EsipaMessageFromEimToIpa_PR_getEimPackageResponse);
	if (!msg_to_ipa)
		return NULL;

	res = IPA_ALLOC_ZERO(struct ipa_esipa_get_eim_pkg_res);
	res->msg_to_ipa = msg_to_ipa;

	switch (msg_to_ipa->choice.getEimPackageResponse.present) {
	case GetEimPackageResponse_PR_euiccPackageRequest:
		res->euicc_package_request = &msg_to_ipa->choice.getEimPackageResponse.choice.euiccPackageRequest;
		break;
	case GetEimPackageResponse_PR_ipaEuiccDataRequest:
		res->ipa_euicc_data_request = &msg_to_ipa->choice.getEimPackageResponse.choice.ipaEuiccDataRequest;
		break;
	case GetEimPackageResponse_PR_profileDownloadTriggerRequest:
		res->dwnld_trigger_request =
		    &msg_to_ipa->choice.getEimPackageResponse.choice.profileDownloadTriggerRequest;
		break;
	case GetEimPackageResponse_PR_eimPackageError:
		res->eim_pkg_err = msg_to_ipa->choice.getEimPackageResponse.choice.eimPackageError;
		IPA_LOGP_ESIPA("GetEimPackage", LERROR, "function failed with error code %ld=%s!\n",
			       res->eim_pkg_err, ipa_str_from_num(error_code_strings, res->eim_pkg_err, "(unknown)"));
		break;
	default:
		IPA_LOGP_ESIPA("GetEimPackage", LERROR, "unexpected response content!\n");
		res->eim_pkg_err = -1;
		break;
	}

	return res;
}

/*! Function (ESipa): GetEimPackage.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_esipa_get_eim_pkg_res *ipa_esipa_get_eim_pkg(struct ipa_context *ctx, const uint8_t *eid)
{
	struct ipa_buf *esipa_req = NULL;
	struct ipa_buf *esipa_res = NULL;
	struct ipa_esipa_get_eim_pkg_res *res = NULL;

	IPA_LOGP_ESIPA("GetEimPackage", LINFO, "Requesting eIM package for eID: %s\n", ipa_hexdump(eid, IPA_LEN_EID));

	esipa_req = enc_get_eim_pkg_req(eid);
	if (!esipa_req)
		goto error;

	esipa_res = ipa_esipa_req(ctx, esipa_req, "GetEimPackage");
	if (!esipa_res)
		goto error;

	res = dec_get_eim_pkg_req(esipa_res);
	if (!res)
		goto error;

error:
	IPA_FREE(esipa_req);
	IPA_FREE(esipa_res);
	return res;
}

/*! Free results of function (ESipa): GetEimPackage.
 *  \param[in] res pointer to function result. */
void ipa_esipa_get_eim_pkg_free(struct ipa_esipa_get_eim_pkg_res *res)
{
	IPA_ESIPA_RES_FREE(res);
}
