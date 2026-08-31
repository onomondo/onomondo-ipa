/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

/* Contract of the weak default allocators (see mem.h). */

#include <assert.h>
#include <string.h>

#include <onomondo/ipa/mem.h>

int main(void)
{
	unsigned char *c;
	char *r;
	void *p;
	int i;

	/* size 0 returns a unique, freeable pointer */
	p = IPA_ALLOC_N(0);
	assert(p);
	IPA_FREE(p);

	/* calloc zeroes */
	c = IPA_CALLOC(4, 8);
	assert(c);
	for (i = 0; i < 32; i++)
		assert(c[i] == 0);
	IPA_FREE(c);

	/* realloc: NULL == malloc, growth preserves content, size 0 stays valid */
	r = IPA_REALLOC(NULL, 4);
	assert(r);
	memcpy(r, "abc", 4);
	r = IPA_REALLOC(r, 4096);
	assert(r);
	assert(strcmp(r, "abc") == 0);
	r = IPA_REALLOC(r, 0);
	assert(r);
	IPA_FREE(r);

	/* free(NULL) is a no-op */
	IPA_FREE(NULL);

	return 0;
}
