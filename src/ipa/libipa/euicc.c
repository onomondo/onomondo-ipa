/*
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/ipad.h>
#include "context.h"
#include "euicc.h"

#define STORE_DATA_CLA 0x80
#define STORE_DATA_INS 0xE2
#define STORE_DATA_P1_LAST_BLOCK 0x91
#define STORE_DATA_P1_MORE_BLOCKS 0x11

#define GET_RESPONSE_CLA 0x00
#define GET_RESPONSE_INS 0xC0

#define SELECT_CLA 0x00
#define SELECT_INS 0xA4

#define MANAGE_CHANNEL_CLA 0x00
#define MANAGE_CHANNEL_INS 0x70

#define MAX_BLOCKSIZE_TX 255
#define MAX_BLOCKSIZE_RX 256

struct req_apdu {
	uint8_t cla;
	uint8_t ins;
	uint8_t p1;
	uint8_t p2;
	uint8_t lc;
	uint16_t le;
	uint8_t data[255];
};

struct res_apdu {
	uint16_t le;
	uint8_t data[255];
	uint16_t sw;
};

/* Format the given req_apdu struct into an IPA_BUF that contains the APDU
 * bytes to send. */
static struct ipa_buf *format_req_apdu(const struct req_apdu *req_apdu)
{
	struct ipa_buf *buf_req = ipa_buf_alloc(5 + req_apdu->lc);
	assert(buf_req);

	buf_req->data[0] = req_apdu->cla;
	buf_req->data[1] = req_apdu->ins;
	buf_req->data[2] = req_apdu->p1;
	buf_req->data[3] = req_apdu->p2;

	if (req_apdu->lc > 0 && req_apdu->le == 0) {
		/* Send data (no response data expected) */
		buf_req->data[4] = req_apdu->lc;
		memcpy(buf_req->data + 5, req_apdu->data, req_apdu->lc);
		buf_req->len = 5 + req_apdu->lc;
	} else if (req_apdu->lc == 0 && req_apdu->le > 0) {
		/* Receive data (no data to send) */
		if (req_apdu->le < 256)
			buf_req->data[4] = req_apdu->le;
		else
			/* See also ETSI TS 102 221, section 10.1.6 */
			buf_req->data[4] = 0;
		buf_req->len = 5;
	} else if (req_apdu->lc == 0 && req_apdu->le == 0) {
		/* No data to send and no receive data expected */
		buf_req->data[4] = 0;
		buf_req->len = 5;
	} else {
		/* The T=0 protocol does not support receiving and sending data
		 * at the same time. The caller must ensure that the APDU
		 * struct is filled in with reasonable values! */
		assert(NULL);
	}

	return buf_req;
}

/* Take the received APDU bytes in res_encoded and parse them into an APDU
 * struct (res_apdu) */
static int parse_res_apdu(struct res_apdu *res_apdu, const struct ipa_buf *res_encoded)
{
	memset(res_apdu, 0, sizeof(*res_apdu));

	/* The encoded response should at least contain 2 byte status word */
	if (res_encoded->len < 2)
		return -EINVAL;

	res_apdu->le = res_encoded->len - 2;
	if (res_apdu->le)
		memcpy(res_apdu->data, res_encoded->data, res_apdu->le);

	res_apdu->sw = res_encoded->data[res_apdu->le] << 8;
	res_apdu->sw |= res_encoded->data[res_apdu->le + 1];

	return 0;
}

static int send_es10x_block(struct ipa_context *ctx, uint16_t *sw,
			    const struct ipa_buf *es10x_req, size_t offset, uint8_t block_nr)
{
	size_t len_req;
	int rc;
	struct req_apdu req_apdu = { 0 };
	struct res_apdu res_apdu = { 0 };
	struct ipa_buf *buf_req = NULL;
	struct ipa_buf *buf_res = NULL;
	uint8_t channel = ctx->cfg->euicc_channel;

	buf_res = ipa_buf_alloc(MAX_BLOCKSIZE_TX + 2);
	assert(buf_res);

	len_req = es10x_req->len - offset;

	/* fill in request APDU for STORE DATA
	 * (see also GSMA SGP.22, section 5.7.2) */
	req_apdu.cla = STORE_DATA_CLA | channel;
	req_apdu.ins = STORE_DATA_INS;
	if (len_req > MAX_BLOCKSIZE_TX)
		req_apdu.p1 = STORE_DATA_P1_MORE_BLOCKS;
	else
		req_apdu.p1 = STORE_DATA_P1_LAST_BLOCK;
	req_apdu.p2 = block_nr;
	if (len_req > MAX_BLOCKSIZE_TX)
		req_apdu.lc = MAX_BLOCKSIZE_TX;
	else
		req_apdu.lc = (uint8_t) len_req;
	memcpy(req_apdu.data, es10x_req->data + offset, req_apdu.lc);

	/* transceive block */
	buf_req = format_req_apdu(&req_apdu);
	rc = ipa_scard_transceive(ctx->scard_ctx, buf_res, buf_req);
	if (rc < 0) {
		IPA_LOGP(SEUICC, LERROR, "unable to send ES10x block %u, offset=%zu\n", block_nr, offset);
		ctx->check_scard = true;
		goto exit;
	}

	/* parse response */
	rc = parse_res_apdu(&res_apdu, buf_res);
	if (rc < 0) {
		IPA_LOGP(SEUICC, LERROR,
			 "invalid response while sending ES10x block %u, offset=%zu\n", block_nr, offset);
		goto exit;
	}
	*sw = res_apdu.sw;

	IPA_LOGP(SEUICC, LINFO, "successfully sent ES10x block %u, offset=%zu, sw=%04x\n", block_nr, offset, *sw);

	/* Return how many data we have sent. */
	rc = req_apdu.lc;
exit:
	IPA_FREE(buf_req);
	IPA_FREE(buf_res);
	return rc;
}

static int recv_es10x_block(struct ipa_context *ctx, uint16_t *sw,
			    struct ipa_buf **es10x_res, uint16_t block_len, uint8_t block_nr)
{
	int rc;
	struct req_apdu req_apdu = { 0 };
	struct res_apdu res_apdu = { 0 };
	struct ipa_buf *buf_req = NULL;
	struct ipa_buf *buf_res = NULL;
	uint8_t channel = ctx->cfg->euicc_channel;
	struct ipa_buf *es10x_res_ptr = *es10x_res;
	size_t realloc_size;

	/* We only support channel 0-3 */
	assert(channel <= 3);

	buf_res = ipa_buf_alloc(MAX_BLOCKSIZE_RX + 2);
	assert(buf_res);

	/* In case the expected block length exceeds our buffer limit, we must
	 * clip. This is no problem since it is always up to the caller to
	 * check by the return code how many bytes were actually transmitted.
	 * The caller also must evaluate the status word to know if there are
	 * still bytes available in the GET RESPONSE buffer of the eUICC. */
	if (block_len > MAX_BLOCKSIZE_RX)
		block_len = MAX_BLOCKSIZE_RX;

	/* fill in request APDU for GET RESPONSE
	 * (see also ISO/IEC 7816-4, 7.6.1) */
	req_apdu.cla = GET_RESPONSE_CLA | channel;
	req_apdu.ins = GET_RESPONSE_INS;
	req_apdu.p1 = 0x00;
	req_apdu.p2 = 0x00;
	req_apdu.lc = 0;
	req_apdu.le = block_len;

	/* receive block */
	buf_req = format_req_apdu(&req_apdu);
	rc = ipa_scard_transceive(ctx->scard_ctx, buf_res, buf_req);
	if (rc < 0) {
		IPA_LOGP(SEUICC, LERROR, "unable to receive ES10x block %u, offset=%zu\n", block_nr,
			 es10x_res_ptr->len);
		ctx->check_scard = true;
		rc = -EIO;
		goto exit;
	}

	/* parse response */
	rc = parse_res_apdu(&res_apdu, buf_res);
	if (rc < 0) {
		IPA_LOGP(SEUICC, LERROR,
			 "invalid response while receiving ES10x block %u, offset=%zu\n", block_nr, es10x_res_ptr->len);
		rc = -EINVAL;
		goto exit;
	}
	if (res_apdu.le != block_len) {
		IPA_LOGP(SEUICC, LERROR,
			 "unexpected block length (expected:%u, got:%u) while sending ES10x block %u, offset=%zu\n",
			 block_len, res_apdu.le, block_nr, es10x_res_ptr->len);
		rc = -EINVAL;
		goto exit;
	}
	if (es10x_res_ptr->len + res_apdu.le > es10x_res_ptr->data_len) {
		realloc_size = ((es10x_res_ptr->len + res_apdu.le) / IPA_LEN_EUICC_BUF + 1) * IPA_LEN_EUICC_BUF;

		IPA_LOGP(SEUICC, LDEBUG,
			 "eUICC response buffer exhausted, reallocating more memory (have: %zu bytes, required: %zu bytes, will allocate: %zu bytes)\n",
			 es10x_res_ptr->data_len, es10x_res_ptr->len + res_apdu.le, realloc_size);

		/* Reallocate the buffer with enough space for one additional block of size MAX_BLOCKSIZE_RX */
		es10x_res_ptr = ipa_buf_realloc(es10x_res_ptr, realloc_size);
		assert(es10x_res_ptr);
	}

	memcpy(es10x_res_ptr->data + es10x_res_ptr->len, res_apdu.data, res_apdu.le);
	es10x_res_ptr->len += res_apdu.le;
	*sw = res_apdu.sw;

	IPA_LOGP(SEUICC, LINFO,
		 "successfully received ES10x block %u, offset=%zu, sw=%04x\n", block_nr, es10x_res_ptr->len, *sw);

	/* Return how many data we have received. */
	rc = res_apdu.le;
exit:
	IPA_FREE(buf_req);
	IPA_FREE(buf_res);
	*es10x_res = es10x_res_ptr;
	return rc;
}

static int euicc_transceive_es10x(struct ipa_context *ctx, struct ipa_buf **es10x_res, const struct ipa_buf *es10x_req)
{
	uint16_t sw;
	uint16_t block_len = 0;
	uint8_t block_nr = 0;
	size_t offset = 0;
	int rc;

	while (1) {
		rc = send_es10x_block(ctx, &sw, es10x_req, offset, block_nr);
		if (rc < 0)
			return -EIO;
		offset += rc;
		block_nr++;

		/* Check if we are done */
		if (offset >= es10x_req->len)
			break;

		/* The eUICC should ACK each block with SW=9000, the last block
		 * be confirmed with 61xx to indicate that response data is
		 * available */
		if (sw != 0x9000 && offset < es10x_req->len) {
			IPA_LOGP(SEUICC, LERROR, "ES10x transmission aborted early by eUICC, sw=%04x\n", sw);
			break;
		}

		/* We can only transmit a maximum amount of 255 blocks in one
		 * STORE DATA cycle. */
		if (block_nr == 255) {
			IPA_LOGP(SEUICC, LERROR,
				 "ES10x request exceeds maximum transmission length (%zu)!\n", es10x_req->len);
			return -EINVAL;
		}
	}

	/* When the transfer of the ES10x request is done, we expect the eUICC
	 * to answer with a response. */
	if (sw == 0x9000) {
		IPA_LOGP(SEUICC, LINFO, "ES10x transmission successful, sw=%04x\n", sw);
		return 0;
	} else if ((sw & 0xff00) == 0x6100) {
		block_nr = 0;

		while (1) {
			/* See also ISO/IEC 7816-4, section 7.4.2 and ETSI TS 102 221, section 10.1.6 */
			if ((sw & 0xff) == 0)
				block_len = 256;
			else
				block_len = sw & 0xff;

			rc = recv_es10x_block(ctx, &sw, es10x_res, block_len, block_nr);
			if (rc < 0)
				return -EIO;
			block_nr++;

			if (sw == 0x9000) {
				IPA_LOGP(SEUICC, LINFO, "ES10x transmission successful, sw=%04x\n", sw);
				return 0;
			}

			if ((sw & 0xff00) != 0x6100) {
				IPA_LOGP(SEUICC, LINFO, "ES10x transmission failed, sw=%04x\n", sw);
				return -EINVAL;
			}
		}
	} else {
		IPA_LOGP(SEUICC, LERROR, "ES10x transmission failed! sw=%04x\n", sw);
		return -EINVAL;
	}

	return 0;
}

/*! Transceive eUICC/es10x APDU.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] es10x_req buffer with eUICC/es10x request.
 *  \returns IPA_BUF with ES10x response on success, NULL on failure. */
struct ipa_buf *ipa_euicc_transceive_es10x(struct ipa_context *ctx, const struct ipa_buf *es10x_req)
{
	struct ipa_buf *es10x_res = ipa_buf_alloc(IPA_LEN_EUICC_BUF);
	int rc;

	IPA_LOGP(SEUICC, LDEBUG, "sending %zu bytes to eUICC (buffer size: %zu bytes)\n", es10x_req->len,
		 es10x_req->data_len);

	rc = euicc_transceive_es10x(ctx, &es10x_res, es10x_req);

	if (rc < 0) {
		IPA_FREE(es10x_res);
		return NULL;
	}

	IPA_LOGP(SEUICC, LDEBUG, "received %zu bytes from eUICC (buffer size: %zu bytes)\n", es10x_res->len,
		 es10x_res->data_len);

	return es10x_res;
}

/* Send terminal capablilities, see also 3gpp TS 102.221 V16.2.0, section 11.1.19.2.4 */
static int send_termcap(struct ipa_context *ctx)
{
	const uint8_t termcap[] = { 0xA9, 0x03, 0x83, 0x01, 0x07 };
	int rc;
	struct req_apdu req_apdu = { 0 };
	struct res_apdu res_apdu = { 0 };
	struct ipa_buf *buf_req = NULL;
	struct ipa_buf *buf_res = NULL;

	buf_res = ipa_buf_alloc(MAX_BLOCKSIZE_RX + 2);
	assert(buf_res);

	/* send TERMINAL CAPABILITIES */
	req_apdu.cla = 0x80;
	req_apdu.ins = 0xAA;
	req_apdu.p1 = 0x00;
	req_apdu.p2 = 0x00;
	req_apdu.lc = sizeof(termcap);
	memcpy(req_apdu.data, termcap, sizeof(termcap));
	buf_req = format_req_apdu(&req_apdu);

	rc = ipa_scard_transceive(ctx->scard_ctx, buf_res, buf_req);
	if (rc < 0) {
		IPA_LOGP(SEUICC, LERROR, "unable to send TERMINAL CAPABILITIES due to communication error\n");
		ctx->check_scard = true;
		rc = -EIO;
		goto exit;
	}

	rc = parse_res_apdu(&res_apdu, buf_res);
	if (rc < 0) {
		IPA_LOGP(SEUICC, LERROR, "invalid response while sending TERMINAL CAPABILITIES\n");
		rc = -EINVAL;
		goto exit;
	}

	if ((res_apdu.sw & 0xFF00) != 0x9000) {
		IPA_LOGP(SEUICC, LERROR, "failed to send TERMINAL CAPABILITIES, sw=%04x\n", res_apdu.sw);
		rc = -EINVAL;
		goto exit;
	}

	IPA_LOGP(SEUICC, LINFO, "TERMINAL CAPABILITIES sent\n");
exit:
	IPA_FREE(buf_req);
	IPA_FREE(buf_res);
	return rc;
}

static int select_isd_r(struct ipa_context *ctx)
{
	const uint8_t aid_isd_r[] =
	    { 0xA0, 0x00, 0x00, 0x05, 0x59, 0x10, 0x10, 0xFF, 0xFF, 0xFF, 0xFF, 0x89, 0x00, 0x00, 0x01, 0x00 };

	int rc;
	struct req_apdu req_apdu = { 0 };
	struct res_apdu res_apdu = { 0 };
	struct ipa_buf *buf_req = NULL;
	struct ipa_buf *buf_res = NULL;
	uint8_t channel = ctx->cfg->euicc_channel;

	/* We only support channel 0-3 */
	assert(channel <= 3);

	buf_res = ipa_buf_alloc(MAX_BLOCKSIZE_RX + 2);
	assert(buf_res);

	/* SELECT ADF.ISD-R */
	req_apdu.cla = SELECT_CLA | channel;
	req_apdu.ins = SELECT_INS;
	req_apdu.p1 = 0x04;
	req_apdu.p2 = 0x04;
	req_apdu.lc = 16;
	req_apdu.le = 0;
	memcpy(req_apdu.data, aid_isd_r, sizeof(aid_isd_r));
	buf_req = format_req_apdu(&req_apdu);

	rc = ipa_scard_transceive(ctx->scard_ctx, buf_res, buf_req);
	if (rc < 0) {
		IPA_LOGP(SEUICC, LERROR, "unable select ISD-R due to communication error\n");
		ctx->check_scard = true;
		rc = -EIO;
		goto exit;
	}

	rc = parse_res_apdu(&res_apdu, buf_res);
	if (rc < 0) {
		IPA_LOGP(SEUICC, LERROR, "invalid response while selecting ISD-R\n");
		rc = -EINVAL;
		goto exit;
	}

	if ((res_apdu.sw & 0xFF00) != 0x6100) {
		IPA_LOGP(SEUICC, LERROR, "failed to select ISD-R, sw=%04x\n", res_apdu.sw);
		rc = -EINVAL;
		goto exit;
	}

	IPA_LOGP(SEUICC, LINFO, "ISD-R selected\n");
exit:
	IPA_FREE(buf_req);
	IPA_FREE(buf_res);
	return rc;

}

static int manage_channel(struct ipa_context *ctx, bool close)
{
	int rc;
	struct req_apdu req_apdu = { 0 };
	struct res_apdu res_apdu = { 0 };
	struct ipa_buf *buf_req = NULL;
	struct ipa_buf *buf_res = NULL;
	uint8_t channel = ctx->cfg->euicc_channel;

	/* We only support channel 0-3 */
	assert(channel <= 3);

	if (channel == 0) {
		IPA_LOGP(SEUICC, LINFO, "using basic logical channel %u, no need to %s a channel\n", channel,
			 close ? "close" : "open");
		return 0;
	}

	buf_res = ipa_buf_alloc(MAX_BLOCKSIZE_RX + 2);
	assert(buf_res);

	/* MANAGE CHANNEL */
	req_apdu.cla = MANAGE_CHANNEL_CLA;
	req_apdu.ins = MANAGE_CHANNEL_INS;
	if (close)
		req_apdu.p1 = 0x80;
	else
		req_apdu.p1 = 0x00;
	req_apdu.p2 = channel;
	req_apdu.lc = 0;
	req_apdu.le = 0;
	buf_req = format_req_apdu(&req_apdu);

	rc = ipa_scard_transceive(ctx->scard_ctx, buf_res, buf_req);
	if (rc < 0) {
		IPA_LOGP(SEUICC, LERROR, "unable %s logical channel %u due to communication error with eUICC\n",
			 close ? "close" : "open", channel);
		ctx->check_scard = true;
		rc = -EIO;
		goto exit;
	}

	rc = parse_res_apdu(&res_apdu, buf_res);
	if (rc < 0) {
		IPA_LOGP(SEUICC, LERROR, "invalid response from eUICC, cannot %s logical channel %u\n",
			 close ? "close" : "open", channel);
		rc = -EINVAL;
		goto exit;
	}

	if ((res_apdu.sw) != 0x9000) {
		IPA_LOGP(SEUICC, LERROR, "failed to %s logical channel %u, sw=%04x\n", close ? "close" : "open",
			 channel, res_apdu.sw);
		rc = -EINVAL;
		goto exit;
	}

	IPA_LOGP(SEUICC, LINFO, "logical channel %u %s\n", channel, close ? "closed" : "opened");
exit:
	IPA_FREE(buf_req);
	IPA_FREE(buf_res);
	return rc;
}

/*! open the communication channel between eUICC and IPAd.
 *  \param[inout] ctx pointer to ipa_context.
 *  \returns 0 on success, negative on error. */
int ipa_euicc_init_es10x(struct ipa_context *ctx)
{
	int rc;

	rc = send_termcap(ctx);
	if (rc < 0)
		return rc;

	rc = manage_channel(ctx, false);
	if (rc < 0)
		return rc;

	rc = select_isd_r(ctx);
	return rc;
}

/*! close the communication channel between eUICC and IPAd.
 *  \param[inout] ctx pointer to ipa_context.
 *  \returns 0 on success, negative on error. */
int ipa_euicc_close_es10x(struct ipa_context *ctx)
{
	return manage_channel(ctx, true);
}
