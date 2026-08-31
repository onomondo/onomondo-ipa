/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 */

#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <onomondo/ipa/utils.h>
#include <asn_application.h>
#include "src/ipa/libipa/utils.h"
#include "src/ipa/libipa/es10x.h"
#include "src/ipa/libipa/bpp_segments.h"

void ipa_bpp_segments_encode_test(char *test_vector_path)
{
	uint8_t bpp[20480];
	FILE *bpp_file = NULL;
	size_t bpp_len;
	struct ipa_bpp_segment_iter iter;
	struct ipa_buf segment;
	unsigned int i;
	int rc;

	/* Load test BPP test vector from file */
	assert(test_vector_path);
	bpp_file = fopen(test_vector_path, "r");
	assert(bpp_file);
	bpp_len = fread(&bpp, sizeof(char), sizeof(bpp), bpp_file);
	fclose(bpp_file);
	printf("bpp size: %zu\n", bpp_len);

	/* Slice BPP segments straight from the wire bytes (one at a time, like
	 * the profile installation procedure does). Calls stay outside the
	 * asserts: NDEBUG builds must still produce the segments (the golden
	 * stderr comparison depends on it). */
	rc = ipa_bpp_segment_iter_init(&iter, bpp, bpp_len);
	assert(rc == 0);
	for (i = 0; i < iter.count; i++) {
		rc = ipa_bpp_segment_iter_next(&iter, &segment);
		assert(rc == 1);
	}
	/* The iterator must report exhaustion once all segments were produced */
	rc = ipa_bpp_segment_iter_next(&iter, &segment);
	assert(rc == 0);
	(void)rc;
}

/* An '88' length that would wrap a 32-bit size_t must be rejected. */
void ipa_bpp_segments_wrap_test(void)
{
	static const uint8_t bpp[] = { 0xBF, 0x36, 0x12, 0xBF, 0x23, 0x00, 0xA0, 0x03, 0x87, 0x01, 0x00,
				       0xA1, 0x06, 0x88, 0x84, 0xFF, 0xFF, 0xFF, 0xFA, 0xA3, 0x00 };
	struct ipa_bpp_segment_iter iter;
	int rc = ipa_bpp_segment_iter_init(&iter, bpp, sizeof(bpp));

	assert(rc == -EINVAL);
	(void)rc;
}

int main(int argc, char **argv)
{
	ipa_bpp_segments_encode_test(argv[1]);
	ipa_bpp_segments_wrap_test();
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
