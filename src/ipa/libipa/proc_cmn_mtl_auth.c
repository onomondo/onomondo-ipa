/*
 * Copyrighct (c) 2025 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, section 3.0.1: Common Mutual Authentication Procedure
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <onomondo/ipa/ipad.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/log.h>
#include <UTF8String.h>
#include "context.h"
#include "utils.h"
#include "es10b_get_euicc_info.h"
#include "es10b_get_euicc_chlg.h"
#include "es10b_auth_serv.h"
#include "esipa_auth_clnt.h"
#include "esipa_init_auth.h"
#include "proc_cmn_cancel_sess.h"
#include "proc_cmn_mtl_auth.h"

/* Walk through the euiccCiPKIdListForVerification list and remove all entries that do not match the given eSIM CA
 * RootCA public key identifier (allowed_ca), See also GSMA SGP.22, section 3.0.1, step 1c. */
static int restrict_euicc_info(struct ipa_es10b_euicc_info *euicc_info, const struct ipa_buf *allowed_ca)
{
	int i;
	struct EUICCInfo1 *asn = euicc_info->euicc_info_1;

	if (!allowed_ca)
		return 0;

	for (i = asn->euiccCiPKIdListForVerification.list.count; i > 0; i--) {
		if (asn->euiccCiPKIdListForVerification.list.array[i - 1]->size != allowed_ca->len
		    || memcmp(asn->euiccCiPKIdListForVerification.list.array[i - 1]->buf, allowed_ca->data,
			      allowed_ca->len)) {
			IPA_FREE(asn->euiccCiPKIdListForVerification.list.array[i - 1]->buf);
			IPA_FREE(asn->euiccCiPKIdListForVerification.list.array[i - 1]);
			asn_sequence_del(&asn->euiccCiPKIdListForVerification, i - 1, 0);
		}
	}

	/* The resulting list must not be empty */
	if (asn->euiccCiPKIdListForVerification.list.count == 0) {
		IPA_LOGP(SMAIN, LERROR, "allowed CA not found in PKI list!\n");
		return -EINVAL;
	}

	return 0;
}

/* Check the certificate for plausibility (see also GSMA SGP.22, section 3.0.1, step #10) */
static int check_certificate(const struct ipa_buf *allowed_ca, const Certificate_t * certificate)
{
	/* GSMA SGP.32, section 3.0.1, step 10 says:
	 * "If there is a restriction to a single allowed eSIM CA RootCA public key identifier, verify that the Subject
	 * Key Identifier of the eUICC RootCA corresponding to CERT.XXauth.SIG matches this value."
	 *
	 * This technically means that we have to look at the Authority Key Identifier of the certificate we have just
	 * received from the eIM, since this value is the same as the value in the Subject Key Identifier of the
	 * eSIM RootCA certificate. */
	if (allowed_ca) {
		const asn_oid_arc_t id_ce_authorityKeyIdentifier[4] = { 2, 5, 29, 35 };
		asn_oid_arc_t extension_arcs[256];
		size_t extension_arcs_len;
		const struct Extensions *extensions;
		bool allowed_ca_present = false;
		int i;
		struct ipa_buf *allowed_ca_tlv;

		/* In the certificate the public key identifier is wrapped in a TLV structure */
		allowed_ca_tlv = ipa_buf_alloc(allowed_ca->len + 4);
		allowed_ca_tlv->len = allowed_ca->len + 4;
		allowed_ca_tlv->data[0] = 0x30;
		allowed_ca_tlv->data[1] = allowed_ca->len + 2;
		allowed_ca_tlv->data[2] = 0x80;
		allowed_ca_tlv->data[3] = allowed_ca->len;
		memcpy(allowed_ca_tlv->data + 4, allowed_ca->data, allowed_ca->len);
		extensions = certificate->tbsCertificate.extensions;
		for (i = 0; i < extensions->list.count; i++) {
			extension_arcs_len =
			    OBJECT_IDENTIFIER_get_arcs(&extensions->list.array[i]->extnID, extension_arcs,
						       IPA_ARRAY_SIZE(extension_arcs));
			if (extension_arcs_len == IPA_ARRAY_SIZE(id_ce_authorityKeyIdentifier) &&
			    memcmp(extension_arcs, id_ce_authorityKeyIdentifier,
				   sizeof(id_ce_authorityKeyIdentifier)) == 0) {
				if (IPA_ASN_STR_CMP_BUF
				    (&extensions->list.array[i]->extnValue, allowed_ca_tlv->data, allowed_ca_tlv->len))
					allowed_ca_present = true;
			}
		}

		IPA_FREE(allowed_ca_tlv);

		if (!allowed_ca_present) {
			IPA_LOGP(SIPA, LERROR,
				 "the CA (authorityKeyIdentifier, 2.5.29.35) of the server certificate does not match the allowed eSIM Root CA (%s)!\n",
				 ipa_buf_hexdump(allowed_ca));
			return -EINVAL;
		}
	}

	/* TODO: Verify that the certificate is valid in respect to its time window. This is an optional step but it
	 * may make sense to catch such problems early. */

	return 0;
}

static void gen_ctx_params_1(struct CtxParams1 *ctx_params_1, const uint8_t *tac, const char *ac_token)
{
	assert(ctx_params_1);
	memset(ctx_params_1, 0, sizeof(*ctx_params_1));

	ctx_params_1->present = CtxParams1_PR_ctxParamsForCommonAuthentication;

	/* Use the AC_TOKEN as MatchingId (see also GSMA SGP.22, section 4.1) */
	if (ac_token) {
		static UTF8String_t matchingId;
		IPA_ASSIGN_STR_TO_ASN(matchingId, ac_token);
		ctx_params_1->choice.ctxParamsForCommonAuthentication.matchingId = &matchingId;
	}

	IPA_ASSIGN_BUF_TO_ASN(ctx_params_1->choice.ctxParamsForCommonAuthentication.deviceInfo.tac, (uint8_t *) tac,
			      IPA_LEN_TAC);

	/* TODO: the IMEI field is optional, do we need it?
	 * ctx_params_1->choice.ctxParamsForCommonAuthentication.deviceInfo.imei = (8 byte octet string, optional); */

	/* TODO: deviceCapabilities is a mandatory field, but the struct it represents contains only optional
	 * fields. Does that mean that we have to populate at least one of those fields?
	 * ctx_params_1->choice.ctxParamsForCommonAuthentication.deviceInfo.deviceCapabilities... = ?; */
}

/*! Perform Common Mutual Authentication Procedure.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] pars pointer to struct that holds the procedure parameters.
 *  \returns pointer newly allocated struct with procedure result, NULL on error. */
struct ipa_esipa_auth_clnt_res *ipa_proc_cmn_mtl_auth(struct ipa_context *ctx,
						      const struct ipa_proc_cmn_mtl_auth_pars *pars)
{
	struct ipa_es10b_euicc_info *euicc_info = NULL;
	uint8_t euicc_challenge[IPA_LEN_SERV_CHLG];
	struct ipa_esipa_init_auth_req init_auth_req = { 0 };
	struct ipa_esipa_init_auth_res *init_auth_res = NULL;;
	struct ipa_es10b_auth_serv_req auth_serv_req = { 0 };
	struct ipa_es10b_auth_serv_res *auth_serv_res = NULL;
	struct ipa_esipa_auth_clnt_req auth_clnt_req = { 0 };
	struct ipa_esipa_auth_clnt_res *auth_clnt_res = NULL;
	struct ipa_proc_cmn_cancel_sess_pars cmn_cancel_sess_pars = { 0 };
	int rc;
	bool exec_cmn_cancel_sess = false;
	IPA_BUF_STATIC(transaction_id, 16);

	/* Step #1 */
	euicc_info = ipa_es10b_get_euicc_info(ctx, false);
	if (!euicc_info)
		goto error;
	rc = restrict_euicc_info(euicc_info, pars->allowed_ca);
	if (rc < 0)
		goto error;

	/* Step #2-#4 */
	rc = ipa_es10b_get_euicc_chlg(ctx, euicc_challenge);
	if (rc < 0)
		goto error;

	/* Step #5-#9 */
	init_auth_req.euicc_challenge = euicc_challenge;
	init_auth_req.smdp_addr = (char *)pars->smdp_addr;
	init_auth_req.euicc_info_1 = euicc_info->euicc_info_1;
	init_auth_res = ipa_esipa_init_auth(ctx, &init_auth_req);
	if (!init_auth_res)
		goto error;
	else if (init_auth_res->init_auth_err)
		goto error;
	else if (!init_auth_res->init_auth_ok)
		goto error;

	/* Pick the transactionId early since we will need it often in the following steps */
	rc = IPA_COPY_ASN_TO_IPA_BUF(&transaction_id, &init_auth_res->init_auth_ok->serverSigned1.transactionId);
	if (rc < 0)
		goto error;

	/* Save some heap memory by freeing early */
	ipa_es10b_get_euicc_info_free(euicc_info);
	euicc_info = NULL;

	/* Step #10 */
	rc = check_certificate(pars->allowed_ca, &init_auth_res->init_auth_ok->serverCertificate);
	if (rc < 0)
		goto error;

	/* Step #11-#14 */
	auth_serv_req.req.serverSigned1 = init_auth_res->init_auth_ok->serverSigned1;
	auth_serv_req.req.serverSignature1 = init_auth_res->init_auth_ok->serverSignature1;
	auth_serv_req.req.serverSignature1.size =
	    ipa_strip_tlv_envelope(auth_serv_req.req.serverSignature1.buf, auth_serv_req.req.serverSignature1.size,
				   0x5f37);
	auth_serv_req.req.euiccCiPKIdToBeUsed = init_auth_res->init_auth_ok->euiccCiPKIdToBeUsed;
	auth_serv_req.req.euiccCiPKIdToBeUsed.size =
	    ipa_strip_tlv_envelope(auth_serv_req.req.euiccCiPKIdToBeUsed.buf,
				   auth_serv_req.req.euiccCiPKIdToBeUsed.size, 0x04);
	auth_serv_req.req.serverCertificate = init_auth_res->init_auth_ok->serverCertificate;
	gen_ctx_params_1(&auth_serv_req.req.ctxParams1, pars->tac, pars->ac_token);
	auth_serv_res = ipa_es10b_auth_serv(ctx, &auth_serv_req);
	if (!auth_serv_res)
		goto error;

	/* Save some heap memory by freeing early */
	ipa_esipa_init_auth_res_free(init_auth_res);
	init_auth_res = NULL;

	/* Step #15-#19 */
	IPA_ASSIGN_IPA_BUF_TO_ASN(auth_clnt_req.req.transactionId, &transaction_id);
	if (auth_serv_res->auth_serv_err) {
		auth_clnt_req.req.authenticateServerResponse.present =
		    SGP32_AuthenticateServerResponse_PR_authenticateResponseError;
		auth_clnt_req.req.authenticateServerResponse.choice.authenticateResponseError.authenticateErrorCode =
		    auth_serv_res->auth_serv_err;
		IPA_ASSIGN_IPA_BUF_TO_ASN(auth_clnt_req.req.authenticateServerResponse.choice.authenticateResponseError.
					  transactionId, &transaction_id);
	} else if (auth_serv_res->auth_serv_ok) {
		auth_clnt_req.req.authenticateServerResponse.present =
		    SGP32_AuthenticateServerResponse_PR_authenticateResponseOk;
		auth_clnt_req.req.authenticateServerResponse.choice.authenticateResponseOk =
		    *auth_serv_res->auth_serv_ok;
	}
	auth_clnt_res = ipa_esipa_auth_clnt(ctx, &auth_clnt_req);
	if (!auth_clnt_res) {
		exec_cmn_cancel_sess = true;
		goto error;
	} else if (auth_clnt_res->auth_clnt_err) {
		exec_cmn_cancel_sess = true;
		goto error;
	} else if (!auth_clnt_res->auth_clnt_ok_dpe && !auth_clnt_res->auth_clnt_ok_dse) {
		exec_cmn_cancel_sess = true;
		goto error;
	}

	ipa_esipa_init_auth_res_free(init_auth_res);
	ipa_es10b_get_euicc_info_free(euicc_info);
	ipa_es10b_auth_serv_res_free(auth_serv_res);
	IPA_LOGP(SIPA, LINFO, "mutual authentication succeeded!\n");
	return auth_clnt_res;
error:
	if (exec_cmn_cancel_sess) {
		cmn_cancel_sess_pars.reason = CancelSessionReason_undefinedReason;
		IPA_ASSIGN_IPA_BUF_TO_ASN(cmn_cancel_sess_pars.transaction_id, &transaction_id);
		ipa_proc_cmn_cancel_sess(ctx, &cmn_cancel_sess_pars);
	}

	ipa_esipa_init_auth_res_free(init_auth_res);
	ipa_es10b_get_euicc_info_free(euicc_info);
	ipa_es10b_auth_serv_res_free(auth_serv_res);
	ipa_esipa_auth_clnt_res_free(auth_clnt_res);
	IPA_LOGP(SIPA, LERROR, "mutual authentication failed!\n");
	return NULL;
}
