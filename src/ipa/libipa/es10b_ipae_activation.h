/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdbool.h>

struct ipa_context;

int ipa_es10b_ipae_activation(struct ipa_context *ctx, bool activate_ipae);
