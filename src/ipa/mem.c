/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

/* Tracking implementation of the memory port (contract in mem.h), linked when
 * MEM_EMIT_DEBUG is on. Zero sizes are bumped to 1: malloc(0) may return NULL
 * and realloc(ptr, 0) may free-and-return-NULL, both of which would break the
 * contract. */

#undef NDEBUG
#include <assert.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include <onomondo/ipa/mem.h>

static long mem_total, mem_peak;

static void mem_track(void *ptr, long delta, const char *what)
{
	mem_total += delta;
	if (mem_total > mem_peak)
		mem_peak = mem_total;
	assert(mem_total >= 0);
	printf("====> %s(%p): %+li bytes, %li bytes total, %li bytes peak\n", what, ptr, delta, mem_total, mem_peak);
}

void *ipa_port_malloc(size_t size)
{
	void *ptr = malloc(size ? size : 1);

	mem_track(ptr, malloc_usable_size(ptr), "malloc");
	return ptr;
}

void *ipa_port_calloc(size_t nmemb, size_t size)
{
	void *ptr = calloc(nmemb ? nmemb : 1, size ? size : 1);

	mem_track(ptr, malloc_usable_size(ptr), "calloc");
	return ptr;
}

void *ipa_port_realloc(void *ptr, size_t size)
{
	long old = malloc_usable_size(ptr);
	void *new_ptr = realloc(ptr, size ? size : 1);

	/* a failed realloc leaves the old block allocated */
	if (new_ptr)
		mem_track(new_ptr, (long)malloc_usable_size(new_ptr) - old, "realloc");
	return new_ptr;
}

void ipa_port_free(void *ptr)
{
	mem_track(ptr, -(long)malloc_usable_size(ptr), "free");
	free(ptr);
}
