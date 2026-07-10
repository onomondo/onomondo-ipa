/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <onomondo/ipa/utils.h>
#include <BoundProfilePackage.h>

/* The segments are produced one at a time so that the caller can free each
 * segment right after it has been sent to the eUICC. Materializing all
 * segments at once would hold a second copy of the profile in memory while
 * the decoded BoundProfilePackage is still resident. */
struct ipa_bpp_segment_iter {
	const struct BoundProfilePackage *bpp;
	unsigned int idx;
	unsigned int count;
};

void ipa_bpp_segment_iter_init(struct ipa_bpp_segment_iter *iter, const struct BoundProfilePackage *bpp);
struct ipa_buf *ipa_bpp_segment_iter_next(struct ipa_bpp_segment_iter *iter);
