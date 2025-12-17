/*
 * Copyrighct (c) 2025 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.32, section 5.14.1: Function (ESipa): InitiateAuthentication
 */

#include <stdint.h>
#include <errno.h>
#include <onomondo/ipa/http.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/ipad.h>
#include <EsipaMessageFromIpaToEim.h>
#include <InitiateAuthenticationRequestEsipa.h>
#include <InitiateAuthenticationResponseEsipa.h>
#include <OCTET_STRING.h>
#include "utils.h"
#include "length.h"
#include "context.h"
#include "esipa.h"
#include "esipa_init_auth.h"

static const struct num_str_map error_code_strings[] = {
	{ InitiateAuthenticationResponseEsipa__initiateAuthenticationErrorEsipa_invalidDpAddress, "invalidDpAddress" },
	{ InitiateAuthenticationResponseEsipa__initiateAuthenticationErrorEsipa_euiccVersionNotSupportedByDp,
	 "euiccVersionNotSupportedByDp" },
	{ InitiateAuthenticationResponseEsipa__initiateAuthenticationErrorEsipa_ciPKIdNotSupported,
	 "ciPKIdNotSupported" },
	{ InitiateAuthenticationResponseEsipa__initiateAuthenticationErrorEsipa_smdpAddressMismatch,
	 "smdpAddressMismatch" },
	{ InitiateAuthenticationResponseEsipa__initiateAuthenticationErrorEsipa_smdpOidMismatch, "smdpOidMismatch" },
	{ 0, NULL }
};

static struct ipa_buf *enc_init_auth_req(const struct ipa_esipa_init_auth_req *req)
{
	struct EsipaMessageFromIpaToEim msg_to_eim = { 0 };
	struct OCTET_STRING smdp_address = { 0 };

	msg_to_eim.present = EsipaMessageFromIpaToEim_PR_initiateAuthenticationRequestEsipa;

	/* add eUICC challenge */
	IPA_ASSIGN_BUF_TO_ASN(msg_to_eim.choice.initiateAuthenticationRequestEsipa.euiccChallenge,
			      (uint8_t *) req->euicc_challenge, IPA_LEN_EUICC_CHLG);

	/* add SMDP addr */
	if (req->smdp_addr) {
		msg_to_eim.choice.initiateAuthenticationRequestEsipa.smdpAddress = &smdp_address;
		IPA_ASSIGN_STR_TO_ASN(smdp_address, req->smdp_addr);
	}

	/* eUICC info */
	msg_to_eim.choice.initiateAuthenticationRequestEsipa.euiccInfo1 = (EUICCInfo1_t *) req->euicc_info_1;

	/* Encode */
	return ipa_esipa_msg_to_eim_enc(&msg_to_eim, "InitiateAuthentication");
}

static struct ipa_esipa_init_auth_res *dec_init_auth_res(const struct ipa_buf *msg_to_ipa_encoded)
{
	struct EsipaMessageFromEimToIpa *msg_to_ipa = NULL;
	struct ipa_esipa_init_auth_res *res = NULL;

	msg_to_ipa =
	    ipa_esipa_msg_to_ipa_dec(msg_to_ipa_encoded, "InitiateAuthentication",
				     EsipaMessageFromEimToIpa_PR_initiateAuthenticationResponseEsipa);
	if (!msg_to_ipa)
		return NULL;

	res = IPA_ALLOC_ZERO(struct ipa_esipa_init_auth_res);
	res->msg_to_ipa = msg_to_ipa;

	switch (msg_to_ipa->choice.initiateAuthenticationResponseEsipa.present) {
	case InitiateAuthenticationResponseEsipa_PR_initiateAuthenticationOkEsipa:
		res->init_auth_ok =
		    &msg_to_ipa->choice.initiateAuthenticationResponseEsipa.choice.initiateAuthenticationOkEsipa;
		break;
	case InitiateAuthenticationResponseEsipa_PR_initiateAuthenticationErrorEsipa:
		res->init_auth_err =
		    msg_to_ipa->choice.initiateAuthenticationResponseEsipa.choice.initiateAuthenticationErrorEsipa;
		IPA_LOGP_ESIPA("InitiateAuthentication", LERROR, "function failed with error code %ld=%s!\n",
			       res->init_auth_err, ipa_str_from_num(error_code_strings, res->init_auth_err,
								    "(unknown)"));
		break;
	default:
		IPA_LOGP_ESIPA("InitiateAuthentication", LERROR, "unexpected response content!\n");
		res->init_auth_err = -1;
		break;
	}

	return res;
}

/*! Function (ESipa): InitiateAuthentication.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_esipa_init_auth_res *ipa_esipa_init_auth(struct ipa_context *ctx, const struct ipa_esipa_init_auth_req *req)
{
	struct ipa_buf *esipa_req = NULL;
	struct ipa_buf *esipa_res = NULL;
	struct ipa_esipa_init_auth_res *res = NULL;

	IPA_LOGP_ESIPA("InitiateAuthentication", LINFO, "Requesting authentication with eUICC challenge: %s\n",
		       ipa_hexdump(req->euicc_challenge, IPA_LEN_EUICC_CHLG));

	esipa_req = enc_init_auth_req(req);
	if (!esipa_req)
		goto error;

	esipa_res = ipa_esipa_req(ctx, esipa_req, "InitiateAuthentication");
	if (!esipa_res)
		goto error;

	res = dec_init_auth_res(esipa_res);
	if (!res)
		goto error;

	/* Make sure that the signed serverAddress matches the SMDP address we have sent in the request. */
	if (res->init_auth_ok && !IPA_ASN_STR_CMP_BUF_I
	    (&res->init_auth_ok->serverSigned1.serverAddress, req->smdp_addr, strlen(req->smdp_addr))) {
		IPA_LOGP_ESIPA("InitiateAuthentication", LERROR,
			       "eIM responded with unexpected serverAddress in serverSigned1 (expected: %s)\n",
			       req->smdp_addr);
		res->init_auth_err = -1;
		goto error;
	}

	/* Make sure the euiccChallenge matches the euiccChallenge we have sent in the request. */
	if (res->init_auth_ok && !IPA_ASN_STR_CMP_BUF
	    (&res->init_auth_ok->serverSigned1.euiccChallenge, req->euicc_challenge, IPA_LEN_EUICC_CHLG)) {
		IPA_LOGP_ESIPA("InitiateAuthentication", LERROR,
			       "eIM responded with unexpected euiccChallenge in serverSigned1 (expected: %s, got: %s)\n",
			       ipa_hexdump(req->euicc_challenge, IPA_LEN_EUICC_CHLG),
			       ipa_hexdump(res->init_auth_ok->serverSigned1.euiccChallenge.buf,
					   res->init_auth_ok->serverSigned1.euiccChallenge.size));
		res->init_auth_err = -1;
		goto error;
	}
error:
	IPA_FREE(esipa_req);
	IPA_FREE(esipa_res);
	return res;
}

/*! Free results of function (ESipa): InitiateAuthentication.
 *  \param[in] res pointer to function result. */
void ipa_esipa_init_auth_res_free(struct ipa_esipa_init_auth_res *res)
{
	IPA_ESIPA_RES_FREE(res);
}
