/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdbool.h>
struct ipa_buf;

/* This is the initial buffer size. The HTTP client will automatically re-alloc more memory if needed. */
#define IPA_LEN_HTTP_RESPONSE_BUF 512	/* bytes */

void *ipa_http_init(const char *cabundle, bool no_verif);
struct ipa_buf *ipa_http_req(void *http_ctx, const struct ipa_buf *req, const char *url);
void ipa_http_close(void *http_ctx);
void ipa_http_free(void *http_ctx);
