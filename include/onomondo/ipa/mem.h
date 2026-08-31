/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stddef.h>

/* Memory port. Weak libc-forwarding defaults live in libipa/mem.c; a platform
 * may override them with strong definitions to give the library its own heap.
 * Contract: size 0 allocates a minimal valid object (never NULL-on-0),
 * realloc(NULL, n) == malloc(n), free(NULL) is a no-op. */
void *ipa_port_malloc(size_t size);
void *ipa_port_calloc(size_t nmemb, size_t size);
void *ipa_port_realloc(void *ptr, size_t size);
void ipa_port_free(void *ptr);

#define IPA_ALLOC(obj) IPA_ALLOC_N(sizeof(obj))
#define IPA_ALLOC_N(n) ipa_port_malloc(n)
#define IPA_CALLOC(nmemb, n) ipa_port_calloc(nmemb, n)
#define IPA_REALLOC(obj, n) ipa_port_realloc(obj, n)
#define IPA_FREE(obj) ipa_port_free(obj)
