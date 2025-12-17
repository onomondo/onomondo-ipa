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
#include <asn_application.h>
#include "src/ipa/libipa/utils.h"
#include "src/ipa/libipa/es10x.h"
#include "src/ipa/libipa/bpp_segments.h"

void ipa_bpp_segments_encode_test(char *test_vector_path)
{
	uint8_t bpp[20480];
	FILE *bpp_file = NULL;
	size_t bpp_len;
	asn_dec_rval_t rc;
	struct BoundProfilePackage *bpp_dec = NULL;
	struct ipa_bpp_segments *segments = NULL;

	/* Load test BPP test vector from file */
	assert(test_vector_path);
	bpp_file = fopen(test_vector_path, "r");
	assert(bpp_file);
	bpp_len = fread(&bpp, sizeof(char), sizeof(bpp), bpp_file);
	fclose(bpp_file);
	printf("bpp size: %zu\n", bpp_len);

	/* Decode BPP test vector */
	rc = ber_decode(0, &asn_DEF_BoundProfilePackage, (void **)&bpp_dec, bpp, bpp_len);
	assert(rc.code == RC_OK);

	/* Generate BPP segments */
	segments = ipa_bpp_segments_encode(bpp_dec);
	assert(segments);
	ipa_bpp_segments_free(segments);

	ASN_STRUCT_FREE(asn_DEF_BoundProfilePackage, bpp_dec);
}

int main(int argc, char **argv)
{
	ipa_bpp_segments_encode_test(argv[1]);
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
