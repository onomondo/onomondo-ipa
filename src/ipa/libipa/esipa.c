/*
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 */

#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <onomondo/ipa/ipad.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/http.h>
#include "esipa.h"
#include "context.h"
#include "utils.h"
#include "length.h"

#define PREFIX_HTTP "http://"
#define PREFIX_HTTPS "https://"
#define SUFFIX "/gsma/rsp2/asn1"

/*! Read the configured eIM URL (FQDN).
 *  \param[in] ctx pointer to ipa_context.
 *  \returns pointer to eIM URL (statically allocated, do not free). */
char *ipa_esipa_get_eim_url(struct ipa_context *ctx)
{
	static char eim_url[IPA_ESIPA_URL_MAXLEN];
	size_t url_len = 0;

	memset(eim_url, 0, sizeof(eim_url));

	/* Make sure we have a reasonable minimum of space available */
	assert(sizeof(eim_url) > 255);

	if (ctx->cfg->eim_disable_ssl)
		strcpy(eim_url, PREFIX_HTTP);
	else
		strcpy(eim_url, PREFIX_HTTPS);

	/* Be sure we don't accidentally overrun the buffer */
	url_len = strlen(eim_url);
	if (!ctx->eim_fqdn)
		return NULL;
	url_len += strlen(ctx->eim_fqdn);
	url_len += strlen(SUFFIX);
	assert(url_len < sizeof(eim_url));

	strcat(eim_url, ctx->eim_fqdn);
	strcat(eim_url, SUFFIX);

	return eim_url;
}

/*! Decode an ASN.1 encoded eIM to IPA message.
 *  \param[in] msg_to_ipa_encoded pointer to ipa_buf that contains the encoded message.
 *  \param[in] function_name name of the ESipa function (for log messages).
 *  \param[in] expected_res_type type of the expected eIM response (for plausibility check).
 *  \returns pointer newly allocated ASN.1 struct that contains the decoded message, NULL on error. */
struct EsipaMessageFromEimToIpa *ipa_esipa_msg_to_ipa_dec(const struct ipa_buf *msg_to_ipa_encoded,
							  const char *function_name,
							  enum EsipaMessageFromEimToIpa_PR expected_res_type)
{
	struct EsipaMessageFromEimToIpa *msg_to_ipa = NULL;
	asn_dec_rval_t rc;

	assert(msg_to_ipa_encoded);

	if (msg_to_ipa_encoded->len == 0) {
		IPA_LOGP_ESIPA(function_name, LERROR, "eIM response contained no data!\n");
		return NULL;
	}

	IPA_LOGP_ESIPA(function_name, LDEBUG, "ESipa message received from eIM:\n");
	ipa_buf_hexdump_multiline(msg_to_ipa_encoded, 64, 1, SESIPA, LDEBUG);

	rc = ber_decode(0, &asn_DEF_EsipaMessageFromEimToIpa,
			(void **)&msg_to_ipa, msg_to_ipa_encoded->data, msg_to_ipa_encoded->len);

	if (rc.code != RC_OK) {
		if (rc.code == RC_FAIL) {
			IPA_LOGP_ESIPA(function_name, LERROR,
				       "cannot decode eIM response! (invalid ASN.1 encoded data)\n");
		} else if (rc.code == RC_WMORE) {
			IPA_LOGP_ESIPA(function_name, LERROR,
				       "cannot decode eIM response! (message seems to be truncated)\n");
		} else {
			IPA_LOGP_ESIPA(function_name, LERROR, "cannot decode eIM response! (unknown cause)\n");
		}

		IPA_LOGP_ESIPA(function_name, LDEBUG, "the following (incomplete) data was decoded:\n");
		ipa_asn1c_dump(&asn_DEF_EsipaMessageFromEimToIpa, msg_to_ipa, 1, SESIPA, LERROR);

		ASN_STRUCT_FREE(asn_DEF_EsipaMessageFromEimToIpa, msg_to_ipa);
		return NULL;
	}

	IPA_LOGP(SESIPA, LDEBUG, " decoded ASN.1:\n");
	ipa_asn1c_dump(&asn_DEF_EsipaMessageFromEimToIpa, msg_to_ipa, 1, SESIPA, LDEBUG);

	if (msg_to_ipa->present != expected_res_type) {
		IPA_LOGP_ESIPA(function_name, LERROR, "unexpected eIM response\n");
		ASN_STRUCT_FREE(asn_DEF_EsipaMessageFromEimToIpa, msg_to_ipa);
		return NULL;
	}

	return msg_to_ipa;
}

/*! Encode an ASN.1 struct that contains an IPA to eIM message.
 *  \param[in] msg_to_eim pointer to ASN.1 struct that contains the IPA to eIM message.
 *  \param[in] function_name name of the ESipa function (for log messages).
 *  \returns pointer newly allocated ipa_buf that contains the encoded message, NULL on error. */
struct ipa_buf *ipa_esipa_msg_to_eim_enc(const struct EsipaMessageFromIpaToEim *msg_to_eim, const char *function_name)
{
	struct ipa_buf *buf_encoded = NULL;
	asn_enc_rval_t rc;

	assert(msg_to_eim);
	assert(msg_to_eim != EsipaMessageFromIpaToEim_PR_NOTHING);

	IPA_LOGP_ESIPA(function_name, LDEBUG, "ESipa message that will be sent to eIM:\n");
	ipa_asn1c_dump(&asn_DEF_EsipaMessageFromIpaToEim, msg_to_eim, 1, SESIPA, LDEBUG);

	rc = der_encode(&asn_DEF_EsipaMessageFromIpaToEim, msg_to_eim, ipa_asn1c_consume_bytes_cb, &buf_encoded);
	if (rc.encoded <= 0) {
		IPA_LOGP_ESIPA(function_name, LERROR, "cannot encode eIM request! rc = %d\n", rc.encoded);
		IPA_FREE(buf_encoded);
		return NULL;
	}

	IPA_LOGP(SES10X, LDEBUG, " encoded ASN.1:\n");
	ipa_buf_hexdump_multiline(buf_encoded, 64, 1, SESIPA, LDEBUG);

	return buf_encoded;
}

/*! Perform a request towards the eIM.
 *  \param[in] ctx pointer to ipa_context.
 *  \param[in] esipa_req ipa_buf with encoded request data
 *  \param[in] function_name name of the ESipa function (for log messages).
 *  \returns pointer newly allocated ipa_buf that contains the encoded response from the eIM, NULL on error. */
struct ipa_buf *ipa_esipa_req(struct ipa_context *ctx, const struct ipa_buf *esipa_req, const char *function_name)
{
	struct ipa_buf *esipa_res;
	unsigned int i;
	unsigned int wait_time;

	if (!esipa_req) {
		IPA_LOGP_ESIPA(function_name, LERROR, "eIM request failed due to missing encoded request data!\n");
		return NULL;
	}

	for (i = 0; i < ctx->cfg->esipa_req_retries + 1; i++) {
		IPA_LOGP_ESIPA(function_name, LDEBUG, "sending %zu bytes to eIM (buffer size: %zu bytes)\n",
			       esipa_req->len, esipa_req->data_len);
		esipa_res = ipa_http_req(ctx->http_ctx, esipa_req, ipa_esipa_get_eim_url(ctx));
		if (!esipa_res && ctx->cfg->esipa_req_retries == 0) {
			IPA_LOGP_ESIPA(function_name, LERROR, "eIM request failed!\n");
			goto error;
		} else if (!esipa_res && i >= ctx->cfg->esipa_req_retries) {
			IPA_LOGP_ESIPA(function_name, LERROR,
				       "eIM request failed, giving up after retrying %u times!\n", i);
			goto error;
		} else if (!esipa_res) {
			wait_time = (i + 1) * (i + 1);
			IPA_LOGP_ESIPA(function_name, LERROR, "eIM request failed, will retry in %u seconds...!\n",
				       wait_time);
			sleep(wait_time);
		} else {
			/* Successful request */
			IPA_LOGP_ESIPA(function_name, LDEBUG, "received %zu bytes from eIM (buffer size: %zu bytes)\n",
				       esipa_res->len, esipa_res->data_len);
			break;
		}
	}

	return esipa_res;
error:
	ctx->check_http = true;
	IPA_FREE(esipa_res);
	return NULL;
}

/*! Close any underlying transport protocol connection towards the eIM
 *  \param[in] ctx pointer to ipa_context. */
void ipa_esipa_close(struct ipa_context *ctx)
{
	ipa_http_close(ctx->http_ctx);
}
