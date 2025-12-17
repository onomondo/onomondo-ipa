/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdlib.h>
#include <malloc.h>

#define IPA_ALLOC(obj) IPA_ALLOC_N(sizeof(obj))

extern long int ___mem_counter;
extern long int ___mem_peak;

#ifdef MEM_EMIT_DEBUG
#define IPA_ALLOC_N(n) ({ \
	void *___ptr;	  \
	___ptr = malloc(n); \
	___mem_counter += malloc_usable_size(___ptr); \
	if (___mem_counter > ___mem_peak) ___mem_peak = ___mem_counter;	\
	printf("====> %p=malloc(%zu): %li bytes total, %li bytes peak\n", \
	       ___ptr, (size_t)n, ___mem_counter, ___mem_peak);	\
	assert(___mem_counter >= 0); \
	___ptr; \
})
#else
#define IPA_ALLOC_N(n) malloc(n)
#endif

#ifdef MEM_EMIT_DEBUG
#define IPA_CALLOC(nmemb, n) ({ \
	void *___ptr;	  \
	___ptr = calloc(nmemb, n);		      \
	___mem_counter += malloc_usable_size(___ptr); \
	if (___mem_counter > ___mem_peak) ___mem_peak = ___mem_counter;	\
	printf("====> %p=calloc(%zu, %ld): %li bytes total, %li bytes peak\n", \
	       ___ptr, (size_t)nmemb, (long unsigned int)n, ___mem_counter, ___mem_peak); \
	assert(___mem_counter >= 0); \
	___ptr; \
})
#else
#define IPA_CALLOC(nmemb, n) calloc(nmemb, n)
#endif

#ifdef MEM_EMIT_DEBUG
#define IPA_REALLOC(obj, n) ({			\
	void *___ptr;	  \
	___mem_counter -= malloc_usable_size(obj); \
	___ptr = realloc(obj, n); \
	___mem_counter += malloc_usable_size(___ptr); \
	if (___mem_counter > ___mem_peak) ___mem_peak = ___mem_counter;	\
	printf("====> %p=realloc(%p, %ld): %li bytes total, %li bytes peak\n", \
	       ___ptr, obj, (long unsigned int)n, ___mem_counter, ___mem_peak); \
	assert(___mem_counter >= 0); \
	___ptr; \
})
#else
#define IPA_REALLOC(obj, n) realloc(obj, n)
#endif

#ifdef MEM_EMIT_DEBUG
#define IPA_FREE(obj) ({ \
	___mem_counter -= malloc_usable_size(obj); \
	printf("====> free(%p): %li bytes total, %li bytes peak, %zu bytes freed\n", \
	       obj, ___mem_counter, ___mem_peak, malloc_usable_size(obj)); \
	assert(___mem_counter >= 0); \
	free(obj); \
})
#else
#define IPA_FREE(obj) free(obj)
#endif
