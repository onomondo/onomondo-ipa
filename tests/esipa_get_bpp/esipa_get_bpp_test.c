/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * ESipa GetBoundProfilePackage response locating (SGP.32 v1.2 6.3.2, AUTOMATIC
 * TAGS): the BoundProfilePackage slice, the error alternative, and rejection
 * of other envelopes. The decoder is static, so this unit includes its source.
 */

#include <stdio.h>
#include <string.h>
#include "../../src/ipa/libipa/esipa_get_bnd_prfle_pkg.c"

/* The included unit reaches esipa.c, which needs the platform HTTP transport;
 * the locator under test never calls it. */
struct ipa_buf *ipa_http_req(void *http_ctx, const struct ipa_buf *req, const char *url)
{
	(void)http_ctx;
	(void)req;
	(void)url;
	return NULL;
}

void ipa_http_close(void *http_ctx)
{
	(void)http_ctx;
}

static int failures;

static void expect(const char *name, int cond, const char *what)
{
	if (!cond) {
		printf("FAIL %s: %s\n", name, what);
		failures++;
	}
}

static struct ipa_esipa_get_bnd_prfle_pkg_res *run(const uint8_t *wire, size_t len)
{
	return dec_get_bnd_prfle_pkg_res(ipa_buf_alloc_data(len, (uint8_t *)wire));
}

int main(void)
{
	/* BoundProfilePackage stand-in: BF36 with four content bytes */
	static const uint8_t bpp[] = { 0xBF, 0x36, 0x04, 0x01, 0x02, 0x03, 0x04 };
	/* BF3A { [0] { [0] transactionId AABB, BF36 ... } } */
	static const uint8_t ok_txid[] = { 0xBF, 0x3A, 0x0D, 0xA0, 0x0B, 0x80, 0x02, 0xAA, 0xBB,
					   0xBF, 0x36, 0x04, 0x01, 0x02, 0x03, 0x04 };
	static const uint8_t ok_plain[] = { 0xBF, 0x3A, 0x09, 0xA0, 0x07, 0xBF, 0x36, 0x04, 0x01, 0x02, 0x03, 0x04 };
	/* BF3A { [1] downloadOrderExpired(6) } */
	static const uint8_t err[] = { 0xBF, 0x3A, 0x03, 0x81, 0x01, 0x06 };
	/* universal tags where AUTOMATIC TAGS demand [0]/[1]: not a valid response */
	static const uint8_t universal[] = { 0xBF, 0x3A, 0x09, 0x30, 0x07, 0xBF, 0x36, 0x04, 0x01, 0x02, 0x03, 0x04 };
	static const uint8_t other[] = { 0xBF, 0x39, 0x03, 0x81, 0x01, 0x06 };
	struct ipa_esipa_get_bnd_prfle_pkg_res *res;

	res = run(ok_txid, sizeof(ok_txid));
	expect("ok_txid", res->get_bnd_prfle_pkg_err == 0 && res->bpp && res->bpp_len == sizeof(bpp) &&
	       !memcmp(res->bpp, bpp, sizeof(bpp)), "BoundProfilePackage slice not found after the transactionId");
	ipa_esipa_get_bnd_prfle_pkg_res_free(res);

	res = run(ok_plain, sizeof(ok_plain));
	expect("ok_plain", res->get_bnd_prfle_pkg_err == 0 && res->bpp && res->bpp_len == sizeof(bpp) &&
	       !memcmp(res->bpp, bpp, sizeof(bpp)), "BoundProfilePackage slice not found");
	ipa_esipa_get_bnd_prfle_pkg_res_free(res);

	res = run(err, sizeof(err));
	expect("err", res->get_bnd_prfle_pkg_err == 6 && !res->bpp, "error alternative not decoded");
	ipa_esipa_get_bnd_prfle_pkg_res_free(res);

	res = run(universal, sizeof(universal));
	expect("universal", res->get_bnd_prfle_pkg_err == -1 && !res->bpp, "universal tags accepted");
	ipa_esipa_get_bnd_prfle_pkg_res_free(res);

	res = run(other, sizeof(other));
	expect("other", res->get_bnd_prfle_pkg_err == -1 && !res->bpp, "other envelope accepted");
	ipa_esipa_get_bnd_prfle_pkg_res_free(res);

	if (failures)
		return 1;
	printf("esipa_get_bpp: all response checks passed\n");
	return 0;
}
