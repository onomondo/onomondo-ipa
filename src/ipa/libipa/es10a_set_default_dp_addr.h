/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <UTF8String.h>

struct ipa_context;

int ipa_es10a_set_default_dp_addr(struct ipa_context *ctx, const UTF8String_t *addr);
