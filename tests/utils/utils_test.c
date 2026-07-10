/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 */

#include <stdio.h>
#include <stdbool.h>
#include <onomondo/ipa/utils.h>
#include "src/ipa/libipa/utils.h"

void ipa_tag_in_taglist_test(void)
{
	uint8_t _tag_list[] = { 0x80, 0xBF, 0x20, 0xBF, 0x22, 0x83, 0x84, 0xA5, 0xA6, 0x88, 0xA9, 0xBF, 0x2B };
	struct ipa_buf *tag_list;
	bool rc;

	tag_list = ipa_buf_alloc_data(sizeof(_tag_list), _tag_list);

	rc = ipa_tag_in_taglist(0x80, tag_list);
	assert(rc == true);
	rc = ipa_tag_in_taglist(0xBF20, tag_list);
	assert(rc == true);
	rc = ipa_tag_in_taglist(0xBF2B, tag_list);
	assert(rc == true);
	rc = ipa_tag_in_taglist(0xA5, tag_list);
	assert(rc == true);
	rc = ipa_tag_in_taglist(0x22, tag_list);
	assert(rc == false);
	rc = ipa_tag_in_taglist(0xBF, tag_list);
	assert(rc == false);
	rc = ipa_tag_in_taglist(0x2B, tag_list);
	assert(rc == false);
	rc = ipa_tag_in_taglist(0xA3, tag_list);
	assert(rc == false);
	rc = ipa_tag_in_taglist(0x81, tag_list);
	assert(rc == false);
	rc = ipa_tag_in_taglist(0xFF, tag_list);
	assert(rc == false);
	rc = ipa_tag_in_taglist(0x00, tag_list);
	assert(rc == false);
	rc = ipa_tag_in_taglist(0xBF23, tag_list);
	assert(rc == false);
	rc = ipa_tag_in_taglist(0xBF00, tag_list);
	assert(rc == false);
	rc = ipa_tag_in_taglist(0xBFFF, tag_list);
	assert(rc == false);

	IPA_FREE(tag_list);
}

void ipa_parse_btlv_hdr_test(void)
{
	/* short form: 5A len 0x10 */
	uint8_t _short_form[] = { 0x5A, 0x10, 0xAA };
	/* two-byte tag, long form 2 length bytes: BF36 len 0x1234 */
	uint8_t _long_form[] = { 0xBF, 0x36, 0x82, 0x12, 0x34, 0xAA };
	/* long form 3 length bytes: value length 0x012345 (> 64 KiB — used to
	 * be silently truncated by the uint16_t length accumulator) */
	uint8_t _len3_form[] = { 0xA1, 0x83, 0x01, 0x23, 0x45, 0xAA };
	/* truncated: long form announcing 2 length bytes but only 1 present */
	uint8_t _truncated[] = { 0x5A, 0x82, 0x01 };
	struct ipa_buf *buf;
	size_t len = 0;
	uint16_t tag = 0;

	buf = ipa_buf_alloc_data(sizeof(_short_form), _short_form);
	assert(ipa_parse_btlv_hdr(&len, &tag, buf) == 2);
	assert(tag == 0x5A && len == 0x10);
	IPA_FREE(buf);

	buf = ipa_buf_alloc_data(sizeof(_long_form), _long_form);
	assert(ipa_parse_btlv_hdr(&len, &tag, buf) == 5);
	assert(tag == 0xBF36 && len == 0x1234);
	IPA_FREE(buf);

	buf = ipa_buf_alloc_data(sizeof(_len3_form), _len3_form);
	assert(ipa_parse_btlv_hdr(&len, &tag, buf) == 5);
	assert(tag == 0xA1 && len == 0x012345);
	IPA_FREE(buf);

	buf = ipa_buf_alloc_data(sizeof(_truncated), _truncated);
	assert(ipa_parse_btlv_hdr(&len, &tag, buf) == 0);
	IPA_FREE(buf);
}

int main(int argc, char **argv)
{
	ipa_tag_in_taglist_test();
	ipa_parse_btlv_hdr_test();
	return 0;
}

/* Stubs */
void *ipa_http_init(const char *cabundle, bool no_verif)
{
	return NULL;
}

struct ipa_buf *ipa_http_req(void *http_ctx, const struct ipa_buf *req, const char *url)
{
	return NULL;
}

void ipa_http_close(void *http_ctx)
{
	return;
}

void ipa_http_free(void *http_ctx)
{
	return;
}

void *ipa_scard_init(unsigned int reader_num)
{
	return NULL;
}

int ipa_scard_reset(void *scard_ctx)
{
	return 0;
}

int ipa_scard_atr(void *scard_ctx, struct ipa_buf *atr)
{
	return 0;
}

int ipa_scard_transceive(void *scard_ctx, struct ipa_buf *res, const struct ipa_buf *req)
{
	return 0;
}

int ipa_scard_free(void *scard_ctx)
{
	return 0;
}
