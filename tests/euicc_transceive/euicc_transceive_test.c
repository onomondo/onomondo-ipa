/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * ES10x transport against a scripted card: the 61xx GET RESPONSE chain, and
 * the single retry when a GET RESPONSE comes back with the status word alone
 * (ETSI TS 102 221, section 7.3.1.1.5).
 */

#include <stdio.h>
#include <string.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/ipad.h>
#include "../../src/ipa/libipa/context.h"
#include "../../src/ipa/libipa/euicc.h"

/* Scripted card: every transceive pops the next response and records the request. */
struct step {
	const uint8_t *res;
	size_t len;
};

static const struct step *script;
static size_t script_len, step_nr;
static uint8_t reqs[8][8];
static size_t req_lens[8];

int ipa_scard_transceive(void *scard_ctx, struct ipa_buf *res, const struct ipa_buf *req)
{
	(void)scard_ctx;
	if (step_nr >= script_len)
		return -1;
	if (step_nr < 8) {
		req_lens[step_nr] = req->len < 8 ? req->len : 8;
		memcpy(reqs[step_nr], req->data, req_lens[step_nr]);
	}
	ipa_buf_cpy(res, script[step_nr].res, script[step_nr].len);
	step_nr++;
	return 0;
}

static int failures;

static void expect(const char *name, int cond, const char *what)
{
	if (!cond) {
		printf("FAIL %s: %s\n", name, what);
		failures++;
	}
}

static struct ipa_buf *run(const struct step *s, size_t n)
{
	static const uint8_t es10x_req[] = { 0xBF, 0x55, 0x00 };
	struct ipa_config cfg = { 0 }; /* euicc_channel 0: basic channel */
	struct ipa_context ctx = { 0 };
	struct ipa_buf *req = ipa_buf_alloc_data(sizeof(es10x_req), (uint8_t *)es10x_req);
	struct ipa_buf *res;

	ctx.cfg = &cfg;
	script = s;
	script_len = n;
	step_nr = 0;
	res = ipa_euicc_transceive_es10x(&ctx, req);
	IPA_FREE(req);
	return res;
}

static const uint8_t pending5[] = { 0x61, 0x05 };
static const uint8_t data5[] = { 0xBF, 0x55, 0x02, 0x80, 0x00, 0x90, 0x00 };
static const uint8_t get_response5[] = { 0x00, 0xC0, 0x00, 0x00, 0x05 };

int main(void)
{
	struct ipa_buf *res;

	/* The first GET RESPONSE returns 61 05 alone; the retry delivers the block. */
	{
		const struct step s[] = { { pending5, 2 }, { pending5, 2 }, { data5, 7 } };
		res = run(s, 3);
		expect("retry", res && res->len == 5 && !memcmp(res->data, data5, 5),
		       "response not delivered after one empty GET RESPONSE");
		expect("retry", step_nr == 3, "APDU count");
		expect("retry", req_lens[1] == 5 && !memcmp(reqs[1], get_response5, 5) && !memcmp(reqs[2], get_response5, 5),
		       "retry is not the same GET RESPONSE");
		if (res)
			IPA_FREE(res);
	}

	/* A second empty answer fails the transfer; no third GET RESPONSE goes out. */
	{
		const struct step s[] = { { pending5, 2 }, { pending5, 2 }, { pending5, 2 }, { data5, 7 } };
		res = run(s, 4);
		expect("twice", res == NULL, "transfer did not fail");
		expect("twice", step_nr == 3, "more than one retry");
		if (res)
			IPA_FREE(res);
	}

	/* An empty answer with a status word other than 61xx fails at once. */
	{
		static const uint8_t err[] = { 0x6F, 0x00 };
		const struct step s[] = { { pending5, 2 }, { err, 2 }, { data5, 7 } };
		res = run(s, 3);
		expect("sw", res == NULL && step_nr == 2, "retried on a non-61xx status word");
		if (res)
			IPA_FREE(res);
	}

	/* The regular chain is untouched: 255 + 3 bytes over two GET RESPONSEs. */
	{
		static const uint8_t pending255[] = { 0x61, 0xFF };
		static uint8_t big[257];
		static const uint8_t tail[] = { 1, 2, 3, 0x90, 0x00 };
		const struct step s[] = { { pending255, 2 }, { big, 257 }, { tail, 5 } };

		memset(big, 0xAA, 255);
		big[255] = 0x61;
		big[256] = 0x03;
		res = run(s, 3);
		expect("chain", res && res->len == 258 && step_nr == 3, "normal 61xx chain broken");
		if (res)
			IPA_FREE(res);
	}

	if (failures)
		return 1;
	printf("euicc_transceive: all transport checks passed\n");
	return 0;
}
