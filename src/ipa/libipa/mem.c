/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#include <stdlib.h>

#include <onomondo/ipa/mem.h>

/* Weak libc-forwarding defaults for the memory port (contract in mem.h).
 * Zero sizes are bumped to 1: malloc(0) may return NULL and realloc(ptr, 0)
 * may free-and-return-NULL, both of which would break the contract. */

#ifdef MEM_EMIT_DEBUG
#include <assert.h>
#include <malloc.h>
#include <stdio.h>

static long mem_total, mem_peak;

static void mem_track(void *ptr, long sign, const char *what)
{
	mem_total += sign * (long)malloc_usable_size(ptr);
	if (mem_total > mem_peak)
		mem_peak = mem_total;
	assert(mem_total >= 0);
	printf("====> %s(%p): %li bytes total, %li bytes peak\n", what, ptr, mem_total, mem_peak);
}
#else
#define mem_track(ptr, sign, what)
#endif

__attribute__((weak)) void *ipa_port_malloc(size_t size)
{
	void *ptr = malloc(size ? size : 1);

	mem_track(ptr, +1, "malloc");
	return ptr;
}

__attribute__((weak)) void *ipa_port_calloc(size_t nmemb, size_t size)
{
	void *ptr = calloc(nmemb ? nmemb : 1, size ? size : 1);

	mem_track(ptr, +1, "calloc");
	return ptr;
}

__attribute__((weak)) void *ipa_port_realloc(void *ptr, size_t size)
{
	void *new_ptr;

	mem_track(ptr, -1, "realloc-old");
	new_ptr = realloc(ptr, size ? size : 1);
	mem_track(new_ptr, +1, "realloc");
	return new_ptr;
}

__attribute__((weak)) void ipa_port_free(void *ptr)
{
	mem_track(ptr, -1, "free");
	free(ptr);
}
