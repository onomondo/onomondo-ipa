/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

/* Strong definitions must displace the weak defaults linked into this binary. */

#include <assert.h>
#include <stdlib.h>

#include <onomondo/ipa/mem.h>

static int hits;

void *ipa_port_malloc(size_t size)
{
	hits++;
	return malloc(size ? size : 1);
}

void *ipa_port_calloc(size_t nmemb, size_t size)
{
	hits++;
	return calloc(nmemb, size);
}

void *ipa_port_realloc(void *ptr, size_t size)
{
	hits++;
	return realloc(ptr, size);
}

void ipa_port_free(void *ptr)
{
	hits++;
	free(ptr);
}

int main(void)
{
	void *p;

	p = IPA_ALLOC_N(16);
	assert(p);
	p = IPA_REALLOC(p, 32);
	assert(p);
	IPA_FREE(p);
	p = IPA_CALLOC(1, 8);
	assert(p);
	IPA_FREE(p);

	assert(hits == 5);
	return 0;
}
