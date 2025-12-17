/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 * Author: Harald Welte <hwelte@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <curl/curl.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/http.h>
#include <onomondo/ipa/http_hdr.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/mem.h>

struct http_ctx {
	bool initialized;
	const char *cabundle;
	bool no_verif;
	CURL *curl;
};

/*! Initialize HTTP client.
 *  \param[in] cabundle path to a CA bundle.
 *  \param[in] no_verif skip SSL certificate verification (insecure).
 *  \returns pointer to newly allocated HTTP client context. */
void *ipa_http_init(const char *cabundle, bool no_verif)
{
	struct http_ctx *ctx = IPA_ALLOC(struct http_ctx);
	assert(ctx);
	memset(ctx, 0, sizeof(*ctx));

	curl_global_init(CURL_GLOBAL_DEFAULT);
	ctx->initialized = true;
	ctx->cabundle = cabundle;
	ctx->no_verif = no_verif;

	IPA_LOGP(SHTTP, LINFO, "HTTP client initialized.\n");

	return ctx;
}

/* Callback function to extract the HTTP response */
static size_t store_response_cb(void *ptr, size_t size, size_t nmemb, void *clientp)
{
	struct ipa_buf *buf = *(struct ipa_buf **)clientp;
	size_t realloc_size;

	if (buf->len + size * nmemb > buf->data_len) {
		realloc_size = ((buf->len + size * nmemb) / IPA_LEN_HTTP_RESPONSE_BUF + 1) * IPA_LEN_HTTP_RESPONSE_BUF;
		IPA_LOGP(SIPA, LDEBUG,
			 "HTTP response buffer exhausted, reallocating more memory (have: %zu bytes, required: %zu bytes, will allocate: %zu bytes)\n",
			 buf->data_len, buf->len + size * nmemb, realloc_size);
		buf = ipa_buf_realloc(buf, realloc_size);
		assert(buf);
		*(struct ipa_buf **)clientp = buf;
	}

	memcpy(buf->data + buf->len, ptr, size * nmemb);
	buf->len += size * nmemb;

	return size * nmemb;
}

/*! Open a TCP connection (if not already present) and Perform HTTP request.
 *  \param[inout] http_ctx HTTP client context.
 *  \param[in] req buffer with HTTP request (POST).
 *  \param[in] url URL with HTTP request.
 *  \returns HTTP response on success, NULL on failure. */
struct ipa_buf *ipa_http_req(void *http_ctx, const struct ipa_buf *req, const char *url)
{
	struct http_ctx *ctx = http_ctx;
	CURLcode rc;
	struct curl_slist *list = NULL;
	struct ipa_buf *res = ipa_buf_alloc(IPA_LEN_HTTP_RESPONSE_BUF);

	assert(ctx->initialized);

	/* Create a new curl context (also represents an ongoing connection) in case it does not exist */
	if (!ctx->curl) {
		ctx->curl = curl_easy_init();
		if (!ctx->curl) {
			IPA_LOGP(SHTTP, LERROR, "internal HTTP-client failure!\n");
			goto error;
		}
	}
	if (ctx->cabundle) {
		rc = curl_easy_setopt(ctx->curl, CURLOPT_CAINFO, ctx->cabundle);
		if (rc != CURLE_OK) {
			IPA_LOGP(SHTTP, LERROR, "internal HTTP-client failure: %s\n", curl_easy_strerror(rc));
			goto error;
		}
	}

	if (ctx->no_verif) {
		/* Bypass SSL certificate verification (only for debug, disable in productive use!) */
		rc = curl_easy_setopt(ctx->curl, CURLOPT_SSL_VERIFYPEER, 0L);
		if (rc != CURLE_OK) {
			IPA_LOGP(SHTTP, LERROR, "internal HTTP-client failure: %s\n", curl_easy_strerror(rc));
			goto error;
		}

		/* Bypass SSL hostname verification (only for debug, disable in productive use!) */
		rc = curl_easy_setopt(ctx->curl, CURLOPT_SSL_VERIFYHOST, 0L);
		if (rc != CURLE_OK) {
			IPA_LOGP(SHTTP, LERROR, "internal HTTP-client failure: %s\n", curl_easy_strerror(rc));
			goto error;
		}
		IPA_LOGP(SHTTP, LINFO, "security disabled: will not verify server certificate and hostname\n");
	}

	/* Setup header, see also SGP.32, section 6.1.1 */
	list = curl_slist_append(list, "Accept:");
	list = curl_slist_append(list, "User-Agent: " IPA_HTTP_USER_AGENT);
	list = curl_slist_append(list, "X-Admin-Protocol: " IPA_HTTP_X_ADMIN_PROTOCOL);
	list = curl_slist_append(list, "Content-Type: " IPA_HTTP_CONTENT_TYPE);
	rc = curl_easy_setopt(ctx->curl, CURLOPT_HTTPHEADER, list);
	if (rc != CURLE_OK) {
		IPA_LOGP(SHTTP, LERROR, "internal HTTP-client failure: %s\n", curl_easy_strerror(rc));
		goto error;
	}

	/* Perform HTTP Request */
	rc = curl_easy_setopt(ctx->curl, CURLOPT_URL, url);
	if (rc != CURLE_OK) {
		IPA_LOGP(SHTTP, LERROR, "internal HTTP-client failure: %s\n", curl_easy_strerror(rc));
		goto error;
	}
	rc = curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDS, req->data);
	if (rc != CURLE_OK) {
		IPA_LOGP(SHTTP, LERROR, "internal HTTP-client failure: %s\n", curl_easy_strerror(rc));
		goto error;
	}
	rc = curl_easy_setopt(ctx->curl, CURLOPT_POSTFIELDSIZE, req->len);
	if (rc != CURLE_OK) {
		IPA_LOGP(SHTTP, LERROR, "internal HTTP-client failure: %s\n", curl_easy_strerror(rc));
		goto error;
	}
	rc = curl_easy_setopt(ctx->curl, CURLOPT_WRITEFUNCTION, store_response_cb);
	if (rc != CURLE_OK) {
		IPA_LOGP(SHTTP, LERROR, "internal HTTP-client failure: %s\n", curl_easy_strerror(rc));
		goto error;
	}
	rc = curl_easy_setopt(ctx->curl, CURLOPT_WRITEDATA, (void *)&res);
	if (rc != CURLE_OK) {
		IPA_LOGP(SHTTP, LERROR, "internal HTTP-client failure: %s\n", curl_easy_strerror(rc));
		goto error;
	}
	rc = curl_easy_setopt(ctx->curl, CURLOPT_TIMEOUT, 5);
	if (rc != CURLE_OK) {
		IPA_LOGP(SHTTP, LERROR, "internal HTTP-client failure: %s\n", curl_easy_strerror(rc));
		goto error;
	}

	rc = curl_easy_perform(ctx->curl);
	if (rc != CURLE_OK) {
		IPA_LOGP(SHTTP, LERROR, "HTTP request to %s failed: %s\n", url, curl_easy_strerror(rc));
		goto error;
	}
	IPA_LOGP(SHTTP, LINFO, "HTTP request to %s successful: %s\n", url, curl_easy_strerror(rc));

	curl_slist_free_all(list);
	return res;
error:
	ipa_http_close(http_ctx);
	curl_slist_free_all(list);
	ipa_buf_free(res);
	return NULL;
}

/*! Close the TCP underlying TCP connection (to be called after the last request).
 *  \param[inout] http_ctx HTTP client context. */
void ipa_http_close(void *http_ctx)
{
	struct http_ctx *ctx = http_ctx;
	if (!ctx->curl)
		return;
	curl_easy_cleanup(ctx->curl);
	ctx->curl = NULL;
}

/*! Free HTTP client.
 *  \param[inout] http_ctx HTTP client context. */
void ipa_http_free(void *http_ctx)
{
	struct http_ctx *ctx = http_ctx;

	if (!http_ctx)
		return;

	ipa_http_close(http_ctx);
	curl_global_cleanup();
	IPA_FREE(ctx);
	IPA_LOGP(SHTTP, LINFO, "HTTP client freed.\n");
}
