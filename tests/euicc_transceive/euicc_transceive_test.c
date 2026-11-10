/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * ES10x transport against a scripted card: the 61xx GET RESPONSE chain, the
 * single retry when a GET RESPONSE comes back with the status word alone
 * (ETSI TS 102 221, section 7.3.1.1.5), and the repeat of a read-only function
 * whose response was lost, and the last STORE DATA block as a Case 4 command (es10x_case4).
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
static size_t req_full_lens[8];
static uint8_t req_last[8];

int ipa_scard_transceive(void *scard_ctx, struct ipa_buf *res, const struct ipa_buf *req)
{
	(void)scard_ctx;
	if (step_nr >= script_len)
		return -1;
	if (step_nr < 8) {
		req_full_lens[step_nr] = req->len;
		req_last[step_nr] = req->data[req->len - 1];
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

/* GetEimConfigurationData only reads; EnableProfile stands for the functions that change the eUICC. */
static const uint8_t read_req[] = { 0xBF, 0x55, 0x00 };
static const uint8_t write_req[] = { 0xBF, 0x31, 0x00 };

static bool case4;

static struct ipa_buf *run_req(const uint8_t *es10x_req, const struct step *s, size_t n)
{
	struct ipa_config cfg = { 0 }; /* euicc_channel 0: basic channel */
	struct ipa_context ctx = { 0 };
	struct ipa_buf *req = ipa_buf_alloc_data(3, (uint8_t *)es10x_req);
	struct ipa_buf *res;

	cfg.es10x_case4 = case4;
	ctx.cfg = &cfg;
	script = s;
	script_len = n;
	step_nr = 0;
	res = ipa_euicc_transceive_es10x(&ctx, req);
	IPA_FREE(req);
	return res;
}

/* The GET RESPONSE cases use a function that is not repeated, so they see the chain alone. */
static struct ipa_buf *run(const struct step *s, size_t n)
{
	return run_req(write_req, s, n);
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

	/* A read-only function whose response is lost (6F00 on GET RESPONSE) is sent again, once. */
	{
		static const uint8_t err[] = { 0x6F, 0x00 };
		const struct step s[] = { { pending5, 2 }, { err, 2 }, { pending5, 2 }, { data5, 7 } };
		res = run_req(read_req, s, 4);
		expect("reissue", res && res->len == 5 && !memcmp(res->data, data5, 5),
		       "read-only function not repeated after a lost response");
		expect("reissue", step_nr == 4 && req_lens[0] == req_lens[2] && !memcmp(reqs[0], reqs[2], req_lens[0]),
		       "the repeat is not the same command");
		if (res)
			IPA_FREE(res);
	}

	/* A second loss fails the function; it is not sent a third time. */
	{
		static const uint8_t err[] = { 0x6D, 0x00 };
		const struct step s[] = { { pending5, 2 }, { err, 2 }, { pending5, 2 }, { err, 2 }, { pending5, 2 },
					  { data5, 7 } };
		res = run_req(read_req, s, 6);
		expect("reissue-once", res == NULL && step_nr == 4, "more than one repeat");
		if (res)
			IPA_FREE(res);
	}

	/* Flag off: the last block is a Case 3 command, 5 + Lc bytes without Le. */
	{
		const struct step s[] = { { pending5, 2 }, { data5, 7 } };
		res = run(s, 2);
		expect("case3", res && res->len == 5 && step_nr == 2, "chain with the flag off");
		expect("case3", req_full_lens[0] == 5 + sizeof(write_req) && req_last[0] == write_req[2],
		       "request has an Le byte with the flag off");
		if (res)
			IPA_FREE(res);
	}

	case4 = true;

	/* Flag on, data and 9000 in the answer to STORE DATA: one exchange, Le '00' after the data. */
	{
		const struct step s[] = { { data5, 7 } };
		res = run(s, 1);
		expect("case4", res && res->len == 5 && !memcmp(res->data, data5, 5), "response not taken from STORE DATA");
		expect("case4", step_nr == 1, "more than one exchange");
		expect("case4", req_full_lens[0] == 6 + sizeof(write_req) && req_last[0] == 0x00 &&
		       reqs[0][4] == sizeof(write_req), "request is not Case 4 with Le '00'");
		if (res)
			IPA_FREE(res);
	}

	/* Flag on, data and 61xx: the rest comes with GET RESPONSE, both parts are returned. */
	{
		static const uint8_t part1[] = { 0xBF, 0x55, 0x02, 0x61, 0x02 };
		static const uint8_t part2[] = { 0x80, 0x00, 0x90, 0x00 };
		static const uint8_t whole[] = { 0xBF, 0x55, 0x02, 0x80, 0x00 };
		const struct step s[] = { { part1, 5 }, { part2, 4 } };
		res = run(s, 2);
		expect("case4-more", res && res->len == 5 && !memcmp(res->data, whole, 5), "parts not concatenated");
		expect("case4-more", step_nr == 2 && req_lens[1] == 5 && reqs[1][1] == 0xC0 && reqs[1][4] == 0x02,
		       "no GET RESPONSE for the rest");
		if (res)
			IPA_FREE(res);
	}

	/* Flag on, a terminal that does not map Case 4 answers 61xx alone: the GET RESPONSE chain as before. */
	{
		const struct step s[] = { { pending5, 2 }, { data5, 7 } };
		res = run(s, 2);
		expect("case4-61xx", res && res->len == 5 && !memcmp(res->data, data5, 5) && step_nr == 2,
		       "GET RESPONSE chain broken");
		if (res)
			IPA_FREE(res);
	}

	if (failures)
		return 1;
	printf("euicc_transceive: all transport checks passed\n");
	return 0;
}
