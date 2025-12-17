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

int main(int argc, char **argv)
{
	ipa_tag_in_taglist_test();
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
