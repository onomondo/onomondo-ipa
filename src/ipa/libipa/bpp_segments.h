/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <onomondo/ipa/utils.h>

/* Every ES8+ segment (GSMA SGP.22, section 2.5.5) is a contiguous byte range
 * of the BoundProfilePackage exactly as it arrived on the wire, so the
 * iterator works on the raw TLV bytes and returns zero-copy views into them:
 * no decoded tree, no re-encoded copies. The wire buffer must stay valid for
 * the lifetime of the iterator. */
struct ipa_bpp_segment_iter {
	const uint8_t *bpp; /* whole BoundProfilePackage TLV (wire bytes) */
	size_t bpp_len;
	unsigned int count; /* total number of segments */
	/* internal: pre-validated region bounds (offsets into bpp) */
	size_t seg0_len;    /* BPP tag+len + InitialiseSecureChannelRequest TLV */
	size_t a0_off, a0_seg_len;
	size_t a1_off, a1_body, a1_end;
	size_t a2_off, a2_seg_len; /* a2_off == 0: absent */
	size_t a3_off, a3_body, a3_end;
	size_t pos;         /* cursor for the '88'/'86' child walks */
	unsigned int state;
	unsigned int sub_idx;
};

/* Validates the BPP structure and precomputes the segment bounds.
 * Returns 0 on success, -EINVAL on a malformed package. */
int ipa_bpp_segment_iter_init(struct ipa_bpp_segment_iter *iter, const uint8_t *bpp, size_t bpp_len);

/* Fills view with the next segment (a borrowed slice — do not free).
 * Returns 1 when a segment was produced, 0 when exhausted. */
int ipa_bpp_segment_iter_next(struct ipa_bpp_segment_iter *iter, struct ipa_buf *view);
