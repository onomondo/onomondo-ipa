/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * ES10b.eUICCMemoryReset response decoding (SGP.32 v1.2 5.9.5, SGP.22 5.7.19).
 * The decoders are static, so this unit includes their source file.
 */

#include <stdio.h>
#include "../../src/ipa/libipa/es10b_euicc_mem_rst.c"

/* The included ES10b entry points reach euicc.c, which needs the platform card
 * transport; the decoders under test never call it. */
int ipa_scard_transceive(void *scard_ctx, struct ipa_buf *res, const struct ipa_buf *req)
{
	(void)scard_ctx;
	(void)res;
	(void)req;
	return -1;
}

static int failures;

static void check(const char *name, int (*dec)(const struct ipa_buf *), const uint8_t *der, size_t len, int want)
{
	struct ipa_buf *buf = ipa_buf_alloc_data(len, (uint8_t *)der);
	int got = dec(buf);

	IPA_FREE(buf);
	if (got != want) {
		printf("FAIL %s: rc=%d, want %d\n", name, got, want);
		failures++;
	}
}

#define CHECK(dec, der, want) check(#der, dec, der, sizeof(der), want)

int main(void)
{
	/* SGP.32: BF64 { resetResult [0], resetEimResult [1] OPTIONAL,
	 * resetImmediateEnableConfigResult [2] OPTIONAL } */
	static const uint8_t ok[] = { 0xBF, 0x64, 0x03, 0x80, 0x01, 0x00 };
	static const uint8_t nothing_to_delete_iec_ok[] = { 0xBF, 0x64, 0x06, 0x80, 0x01, 0x01, 0x82, 0x01, 0x00 };
	static const uint8_t all_results_ok[] = { 0xBF, 0x64, 0x09, 0x80, 0x01, 0x00,
						   0x81, 0x01, 0x01, 0x82, 0x01, 0x00 };
	static const uint8_t eim_reset_not_supported[] = { 0xBF, 0x64, 0x06, 0x80, 0x01, 0x00, 0x81, 0x01, 0x02 };
	static const uint8_t iec_not_supported[] = { 0xBF, 0x64, 0x06, 0x80, 0x01, 0x00, 0x82, 0x01, 0x01 };
	static const uint8_t cat_busy[] = { 0xBF, 0x64, 0x03, 0x80, 0x01, 0x05 };
	/* SGP.22: BF34 { resetResult [0] } */
	static const uint8_t sgp22_ok[] = { 0xBF, 0x34, 0x03, 0x80, 0x01, 0x00 };
	static const uint8_t sgp22_cat_busy[] = { 0xBF, 0x34, 0x03, 0x80, 0x01, 0x05 };

	CHECK(dec_euicc_mem_rst_res_sgp32, ok, 0);
	CHECK(dec_euicc_mem_rst_res_sgp32, nothing_to_delete_iec_ok, 0);
	CHECK(dec_euicc_mem_rst_res_sgp32, all_results_ok, 0);
	CHECK(dec_euicc_mem_rst_res_sgp32, eim_reset_not_supported, -EINVAL);
	CHECK(dec_euicc_mem_rst_res_sgp32, iec_not_supported, -EINVAL);
	CHECK(dec_euicc_mem_rst_res_sgp32, cat_busy, -EINVAL);
	CHECK(dec_euicc_mem_rst_res, sgp22_ok, 0);
	CHECK(dec_euicc_mem_rst_res, sgp22_cat_busy, -EINVAL);

	if (failures)
		return 1;
	printf("es10b_mem_rst: all decoder checks passed\n");
	return 0;
}
