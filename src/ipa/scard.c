/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <wintypes.h>
#include <winscard.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/mem.h>

#define PCSC_ERROR(reader_num, rv, text) \
if (rv != SCARD_S_SUCCESS) { \
	IPA_LOGP(SSCARD, LERROR, "PCSC reader #%d error: %s (%s,0x%lX)\n", \
		 reader_num, pcsc_stringify_error(rv), text, rv); \
	goto error; \
}

struct scard_ctx {
	bool initialized;
	unsigned int reader_num;
	SCARDCONTEXT hContext;
	DWORD dwActiveProtocol;
	SCARDHANDLE hCard;
	SCARD_IO_REQUEST pioRecvPci;
	const SCARD_IO_REQUEST *pioSendPci;
};

/*! Initialize smartcard reader (and card).
 *  \param[in] reader_num device number of the smartcard reader.
 *  \returns pointer to newly allocated smartcard reader context. */
void *ipa_scard_init(unsigned int reader_num)
{
	struct scard_ctx *ctx;
	long rc;
	LPSTR mszReaders = NULL;
	DWORD dwReaders;
	unsigned int num_readers;
	char *reader_name;

	ctx = IPA_ALLOC_ZERO(struct scard_ctx);
	ctx->reader_num = reader_num;

	/* Initialize reader */
	rc = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &ctx->hContext);
	PCSC_ERROR(reader_num, rc, "SCardEstablishContext");

	dwReaders = SCARD_AUTOALLOCATE;
	rc = SCardListReaders(ctx->hContext, NULL, (LPSTR) & mszReaders, &dwReaders);
	PCSC_ERROR(reader_num, rc, "SCardListReaders");

	num_readers = 0;
	reader_name = mszReaders;
	while (*reader_name != '\0' && num_readers != reader_num) {
		reader_name += strlen(reader_name) + 1;
		num_readers++;
	}
	if (reader_num != num_readers) {
		IPA_LOGP(SSCARD, LINFO, "no smartcard reader.\n");
		goto error;
	}

	/* Initialize card */
	rc = SCardConnect(ctx->hContext, reader_name, SCARD_SHARE_SHARED,
			  SCARD_PROTOCOL_T0, &ctx->hCard, &ctx->dwActiveProtocol);
	PCSC_ERROR(reader_num, rc, "SCardConnect");
	ctx->pioSendPci = SCARD_PCI_T0;

	IPA_LOGP(SSCARD, LINFO, "PCSC reader #%d (%s) initialized.\n", reader_num, reader_name);
	ctx->initialized = true;

	SCardFreeMemory(ctx->hContext, mszReaders);
	return ctx;
error:
	IPA_LOGP(SSCARD, LERROR, "PCSC reader #%d initialization failed!\n", reader_num);
	if (mszReaders)
		SCardFreeMemory(ctx->hContext, mszReaders);
	IPA_FREE(ctx);
	return NULL;
}

/*! Transceive smartcard APDU.
 *  \param[inout] scard_ctx smartcard reader context.
 *  \param[out] res buffer to store smartcard response.
 *  \param[out] req buffer with smartcard request.
 *  \returns 0 on success, -EIO on failure. */
int ipa_scard_transceive(void *scard_ctx, struct ipa_buf *res, const struct ipa_buf *req)
{
	struct scard_ctx *ctx = scard_ctx;
	LONG rc;
	assert(ctx);

	assert(res);
	assert(req);

	res->len = res->data_len;
	IPA_LOGP(SSCARD, LDEBUG, "PCSC reader #%d TX: \n", ctx->reader_num);
	ipa_buf_hexdump_multiline(req, 64, 1, SSCARD, LINFO);

	rc = SCardTransmit(ctx->hCard, ctx->pioSendPci, req->data, req->len, &ctx->pioRecvPci, res->data, (LPDWORD) &res->len);
	PCSC_ERROR(ctx->reader_num, rc, "SCardEndTransaction");

	if (res->len) {
		IPA_LOGP(SSCARD, LDEBUG, "PCSC reader #%d RX: \n", ctx->reader_num);
		ipa_buf_hexdump_multiline(res, 64, 1, SSCARD, LINFO);
	} else {
		IPA_LOGP(SSCARD, LDEBUG, "PCSC reader #%d RX: (no data)\n", ctx->reader_num);
	}

	return 0;
error:
	IPA_LOGP(SSCARD, LERROR, "PCSC reader #%d transceive failed!\n", ctx->reader_num);
	return -EIO;
}

/*! Reset smartcard.
 *  \param[inout] scard_ctx smartcard reader context.
 *  \returns 0 on success, -EIO on failure. */
int ipa_scard_reset(void *scard_ctx)
{
	struct scard_ctx *ctx = scard_ctx;
	LONG rc;
	assert(ctx);

	rc = SCardReconnect(ctx->hCard, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0,
			    SCARD_RESET_CARD, &ctx->dwActiveProtocol);
	PCSC_ERROR(ctx->reader_num, rc, "SCardReconnect");
	IPA_LOGP(SSCARD, LINFO, "PCSC reader #%d card reset\n", ctx->reader_num);
	return 0;
error:
	IPA_LOGP(SSCARD, LERROR, "PCSC reader #%d reset failed!\n", ctx->reader_num);
	return -EIO;
}

/*! Read smartcard ATR.
 *  \param[inout] scard_ctx smartcard reader context.
 *  \param[out] res buffer to store the ATR.
 *  \returns 0 on success, -EIO on failure. */
int ipa_scard_atr(void *scard_ctx, struct ipa_buf *atr)
{
	/* Needed by SCardStatus, but we are not interested in those values here */
	char pbReader[MAX_READERNAME];
	DWORD dwReaderLen = sizeof(pbReader);
	DWORD dwState;
	DWORD dwProt;

	struct scard_ctx *ctx = scard_ctx;
	LONG rc;
	assert(ctx);

	atr->len = atr->data_len;
	rc = SCardStatus(ctx->hCard, pbReader, &dwReaderLen, &dwState, &dwProt, atr->data, (LPDWORD) &atr->len);
	PCSC_ERROR(ctx->reader_num, rc, "SCardStatus");
	IPA_LOGP(SSCARD, LINFO, "PCSC reader #%d ATR:%s\n", ctx->reader_num, ipa_buf_hexdump(atr));
	return 0;
error:
	IPA_LOGP(SSCARD, LERROR, "PCSC reader #%d atr query failed!\n", ctx->reader_num);
	return -EIO;
}

/*! Free smartcard reader (and card).
 *  \param[inout] scard_ctx smartcard reader context.
 *  \returns 0 on success, -EIO on failure. */
int ipa_scard_free(void *scard_ctx)
{
	struct scard_ctx *ctx = scard_ctx;
	LONG rc;

	if (!scard_ctx)
		return 0;

	rc = SCardDisconnect(ctx->hCard, SCARD_UNPOWER_CARD);
	PCSC_ERROR(ctx->reader_num, rc, "SCardDisconnect");

	rc = SCardReleaseContext(ctx->hContext);
	PCSC_ERROR(ctx->reader_num, rc, "SCardReleaseContext");

	IPA_LOGP(SSCARD, LINFO, "PCSC reader #%d freed\n", ctx->reader_num);

	IPA_FREE(ctx);
	return 0;
error:
	IPA_LOGP(SSCARD, LERROR, "PCSC reader #%d free failed!\n", ctx->reader_num);
	return -EIO;
}
