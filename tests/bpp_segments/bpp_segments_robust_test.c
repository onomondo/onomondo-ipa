/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * The BoundProfilePackage slicer works on bytes received from the eIM, so it
 * must reject every truncated package and never hand out a slice outside the
 * buffer, whatever the bytes are. Run under AddressSanitizer the exact-size
 * heap copies also catch reads past the end. The slicer source is included
 * with its log lines and segment hexdumps defined away, the input would
 * otherwise print gigabytes.
 */

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/utils.h>
#undef IPA_LOGP
#define IPA_LOGP(subsys, level, fmt, args...) ((void)0)
#define ipa_buf_hexdump_multiline(buf, width, indent, subsys, level) ((void)0)
#include "../../src/ipa/libipa/bpp_segments.c"

static uint8_t bpp[20480];
static size_t bpp_len;

/* A package cut short at any length must be refused before any segment is produced. */
static void truncated(void)
{
	struct ipa_bpp_segment_iter iter;
	size_t len;

	for (len = 0; len < bpp_len; len++) {
		uint8_t *copy = malloc(len ? len : 1);
		int rc;

		assert(copy);
		memcpy(copy, bpp, len);
		rc = ipa_bpp_segment_iter_init(&iter, copy, len);
		if (rc != -EINVAL) {
			printf("truncated to %zu bytes: rc=%d\n", len, rc);
			exit(1);
		}
		free(copy);
	}
}

/* Packages with a few flipped bytes are either refused or sliced inside the buffer. */
static void corrupted(void)
{
	struct ipa_bpp_segment_iter iter;
	struct ipa_buf segment;
	uint32_t seed = 1;
	unsigned int accepted = 0;
	unsigned int i, k, n;

	for (i = 0; i < 200000; i++) {
		uint8_t *copy = malloc(bpp_len);

		assert(copy);
		memcpy(copy, bpp, bpp_len);
		for (k = 0; k < 1 + i % 4; k++) {
			/* fixed linear congruential generator, so a failure reproduces */
			seed = seed * 1103515245u + 12345u;
			/* bias towards the header area, where the structure lives */
			size_t pos = (seed >> 8) % (i % 2 ? bpp_len : 512);

			seed = seed * 1103515245u + 12345u;
			copy[pos] = (uint8_t)(seed >> 16);
		}

		if (ipa_bpp_segment_iter_init(&iter, copy, bpp_len) == 0) {
			n = 0;
			while (ipa_bpp_segment_iter_next(&iter, &segment) == 1) {
				if (segment.data < copy || segment.len > bpp_len ||
				    (size_t)(segment.data - copy) > bpp_len - segment.len) {
					printf("mutation %u: segment %u outside the package\n", i, n);
					exit(1);
				}
				n++;
			}
			if (n != iter.count) {
				printf("mutation %u: %u segments, %u announced\n", i, n, iter.count);
				exit(1);
			}
			accepted++;
		}
		free(copy);
	}
	printf("corrupted: %u of %u still well-formed\n", accepted, i);
}

int main(int argc, char **argv)
{
	FILE *bpp_file;

	assert(argc == 2);
	bpp_file = fopen(argv[1], "r");
	assert(bpp_file);
	bpp_len = fread(bpp, 1, sizeof(bpp), bpp_file);
	fclose(bpp_file);
	assert(bpp_len > 0 && bpp_len < sizeof(bpp));

	truncated();
	corrupted();
	return 0;
}

/* Stubs: libipa references the platform ports, the slicer never calls them. */
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
}

void ipa_http_free(void *http_ctx)
{
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
