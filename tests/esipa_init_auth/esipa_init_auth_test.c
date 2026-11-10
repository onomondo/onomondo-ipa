/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * ESipa.InitiateAuthentication echoes the eimTransactionId of the ProfileDownloadTriggerRequest
 * (SGP.32 v1.2 section 5.14.1). The test provides ipa_http_req itself, so it sees the bytes libipa
 * sends to the eIM.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <onomondo/ipa/ipad.h>
#include <onomondo/ipa/http.h>
#include <onomondo/ipa/utils.h>
#include <EsipaMessageFromIpaToEim.h>
#include "context.h"
#include "esipa_init_auth.h"

static struct ipa_buf *sent;

/* Stand-in for the HTTP layer: keep the request, answer nothing. */
struct ipa_buf *ipa_http_req(void *http_ctx, const struct ipa_buf *req, const char *url)
{
	(void)http_ctx;
	(void)url;
	IPA_FREE(sent);
	sent = ipa_buf_alloc_and_cpy(req->data, req->len);
	return NULL;
}

void ipa_http_close(void *http_ctx)
{
	(void)http_ctx;
}

static const uint8_t euicc_challenge[16] = {
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF
};
static const uint8_t eim_txid[16] = {
	0xE1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
};

/* Run ESipa.InitiateAuthentication and decode what went to the eIM. */
static struct EsipaMessageFromIpaToEim *init_auth_sent(const TransactionId_t *eim_transaction_id)
{
	struct ipa_config cfg = { 0 };
	struct ipa_context ctx = { 0 };
	struct ipa_esipa_init_auth_req req = { 0 };
	struct EsipaMessageFromIpaToEim *msg = NULL;
	asn_dec_rval_t rc;

	cfg.eim_disable_ssl = true;
	cfg.esipa_req_retries = 0;
	ctx.cfg = &cfg;
	ctx.eim_fqdn = "eim.example.com";

	req.euicc_challenge = euicc_challenge;
	req.smdp_addr = "smdp.example.com";
	req.eim_transaction_id = eim_transaction_id;

	/* the stand-in HTTP layer answers nothing, so the function itself fails */
	assert(ipa_esipa_init_auth(&ctx, &req) == NULL);
	assert(sent);

	rc = ber_decode(NULL, &asn_DEF_EsipaMessageFromIpaToEim, (void **)&msg, sent->data, sent->len);
	assert(rc.code == RC_OK);
	assert(msg->present == EsipaMessageFromIpaToEim_PR_initiateAuthenticationRequestEsipa);
	return msg;
}

static void echoes_eim_transaction_id(void)
{
	TransactionId_t txid = { 0 };
	struct EsipaMessageFromIpaToEim *msg;
	const InitiateAuthenticationRequestEsipa_t *init_auth;

	txid.buf = (uint8_t *)eim_txid;
	txid.size = sizeof(eim_txid);
	msg = init_auth_sent(&txid);
	init_auth = &msg->choice.initiateAuthenticationRequestEsipa;

	assert(init_auth->eimTransactionId);
	assert(init_auth->eimTransactionId->size == sizeof(eim_txid));
	assert(memcmp(init_auth->eimTransactionId->buf, eim_txid, sizeof(eim_txid)) == 0);
	assert(init_auth->smdpAddress);
	assert(init_auth->euiccChallenge.size == sizeof(euicc_challenge));

	/* on the wire as eimTransactionId [2]: '82' 10 <id> */
	assert(sent->len > 18);
	bool found = false;
	for (size_t i = 0; i + 18 <= sent->len; i++) {
		if (sent->data[i] == 0x82 && sent->data[i + 1] == 0x10 &&
		    memcmp(&sent->data[i + 2], eim_txid, sizeof(eim_txid)) == 0)
			found = true;
	}
	assert(found);

	ASN_STRUCT_FREE(asn_DEF_EsipaMessageFromIpaToEim, msg);
	printf("%s: ok\n", __func__);
}

/* A session that no eIM Package triggered (e.g. one the IPA starts itself) has no eimTransactionId to echo. */
static void omits_absent_eim_transaction_id(void)
{
	struct EsipaMessageFromIpaToEim *msg = init_auth_sent(NULL);

	assert(msg->choice.initiateAuthenticationRequestEsipa.eimTransactionId == NULL);

	ASN_STRUCT_FREE(asn_DEF_EsipaMessageFromIpaToEim, msg);
	printf("%s: ok\n", __func__);
}

int main(void)
{
	echoes_eim_transaction_id();
	omits_absent_eim_transaction_id();
	IPA_FREE(sent);
	return 0;
}
