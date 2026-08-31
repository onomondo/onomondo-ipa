/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * The segmentation rules of GSMA SGP.22, section 2.5.5 map every segment onto
 * a contiguous byte range of the BoundProfilePackage wire encoding, so the
 * iterator slices the raw TLV instead of decoding a tree and re-encoding each
 * segment (which briefly held the whole profile in memory twice).
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/utils.h>
#include "utils.h"
#include "bpp_segments.h"

/* Wire tags of BoundProfilePackage and its children (SGP.22 section 2.5.5) */
#define TAG_BPP      0xBF36
#define TAG_ISCR     0xBF23 /* initialiseSecureChannelRequest */
#define TAG_FIRST87  0xA0   /* firstSequenceOf87 */
#define TAG_SEQ88    0xA1   /* sequenceOf88 */
#define TAG_SECOND87 0xA2   /* secondSequenceOf87 (optional) */
#define TAG_SEQ86    0xA3   /* sequenceOf86 */

enum bpp_iter_state {
	SEG_ISCR = 0,
	SEG_FIRST87,
	SEG_HDR88,
	SEG_EACH88,
	SEG_SECOND87,
	SEG_HDR86,
	SEG_EACH86,
	SEG_DONE,
};

/* Validate a sequence region: every child must carry child_tag; returns the
 * child count, negative on a malformed child. */
static int count_children(const uint8_t *bpp, size_t body, size_t end, uint16_t child_tag)
{
	int n = 0;
	size_t pos = body;

	while (pos < end) {
		uint16_t tag;
		size_t val_len;
		size_t hdr_len = ipa_tlv_hdr(bpp + pos, end - pos, &tag, &val_len);

		if (!hdr_len || tag != child_tag)
			return -EINVAL;
		pos += hdr_len + val_len;
		n++;
	}
	return n;
}

/* Parse the child TLV at *off, check its tag, advance *off past it and return
 * its total length (header + value), 0 on error. */
static size_t expect_child(const uint8_t *bpp, size_t *off, size_t end, uint16_t tag_wanted)
{
	uint16_t tag;
	size_t val_len;
	size_t hdr_len;

	if (*off >= end)
		return 0;
	hdr_len = ipa_tlv_hdr(bpp + *off, end - *off, &tag, &val_len);
	if (!hdr_len || tag != tag_wanted)
		return 0;
	*off += hdr_len + val_len;
	return hdr_len + val_len;
}

/* Length of the "sequence tag+len + its first '87' TLV" segment, 0 if the
 * sequence is empty or malformed. */
static size_t first87_seg_len(const uint8_t *seq, size_t hdr_len, size_t val_len, const char *label)
{
	uint16_t tag;
	size_t h87, v87;

	h87 = ipa_tlv_hdr(seq + hdr_len, val_len, &tag, &v87);
	if (!h87 || tag != 0x87)
		return 0;
	if (h87 + v87 < val_len) {
		/* The spec explicitly states "first '87' TLV"! */
		IPA_LOGP(SIPA, LDEBUG, "ignoring excess items in the %s!\n", label);
	}
	return hdr_len + h87 + v87;
}

int ipa_bpp_segment_iter_init(struct ipa_bpp_segment_iter *iter, const uint8_t *bpp, size_t bpp_len)
{
	uint16_t tag;
	size_t val_len, hdr_len, tlv_len, off;
	int n88, n86;

	memset(iter, 0, sizeof(*iter));
	iter->bpp = bpp;
	iter->bpp_len = bpp_len;

	/* Outer BoundProfilePackage TLV */
	hdr_len = ipa_tlv_hdr(bpp, bpp_len, &tag, &val_len);
	if (!hdr_len || tag != TAG_BPP || hdr_len + val_len != bpp_len) {
		IPA_LOGP(SIPA, LERROR, "malformed BoundProfilePackage TLV!\n");
		return -EINVAL;
	}
	off = hdr_len;

	/* Segment: BPP tag+len + the InitialiseSecureChannelRequest TLV */
	tlv_len = expect_child(bpp, &off, bpp_len, TAG_ISCR);
	if (!tlv_len) {
		IPA_LOGP(SIPA, LERROR, "malformed InitialiseSecureChannelRequest!\n");
		return -EINVAL;
	}
	iter->seg0_len = off;

	/* Segment: firstSequenceOf87 tag+len + its first '87' TLV */
	iter->a0_off = off;
	hdr_len = ipa_tlv_hdr(bpp + off, bpp_len - off, &tag, &val_len);
	if (!hdr_len || tag != TAG_FIRST87) {
		IPA_LOGP(SIPA, LERROR, "malformed FirstSequenceOf87!\n");
		return -EINVAL;
	}
	iter->a0_seg_len = first87_seg_len(bpp + off, hdr_len, val_len, "FirstSequenceOf87");
	if (!iter->a0_seg_len) {
		IPA_LOGP(SIPA, LERROR, "cannot slice segment for FirstSequenceOf87 (empty sequence)!\n");
		return -EINVAL;
	}
	off += hdr_len + val_len;

	/* sequenceOf88: header segment + one segment per '88' TLV */
	iter->a1_off = off;
	hdr_len = ipa_tlv_hdr(bpp + off, bpp_len - off, &tag, &val_len);
	if (!hdr_len || tag != TAG_SEQ88) {
		IPA_LOGP(SIPA, LERROR, "malformed SequenceOf88!\n");
		return -EINVAL;
	}
	iter->a1_body = off + hdr_len;
	iter->a1_end = off + hdr_len + val_len;
	n88 = count_children(bpp, iter->a1_body, iter->a1_end, 0x88);
	if (n88 < 0) {
		IPA_LOGP(SIPA, LERROR, "malformed '88' TLV in SequenceOf88!\n");
		return -EINVAL;
	}
	off = iter->a1_end;

	/* secondSequenceOf87 (optional): tag+len + its first '87' TLV */
	hdr_len = ipa_tlv_hdr(bpp + off, bpp_len - off, &tag, &val_len);
	if (hdr_len && tag == TAG_SECOND87) {
		iter->a2_off = off;
		iter->a2_seg_len = first87_seg_len(bpp + off, hdr_len, val_len, "SecondSequenceOf87");
		if (!iter->a2_seg_len) {
			IPA_LOGP(SIPA, LERROR, "cannot slice segment for SecondSequenceOf87 (empty sequence)!\n");
			return -EINVAL;
		}
		off += hdr_len + val_len;
	}

	/* sequenceOf86: header segment + one segment per '86' TLV */
	iter->a3_off = off;
	hdr_len = ipa_tlv_hdr(bpp + off, bpp_len - off, &tag, &val_len);
	if (!hdr_len || tag != TAG_SEQ86 || off + hdr_len + val_len != bpp_len) {
		IPA_LOGP(SIPA, LERROR, "malformed SequenceOf86!\n");
		return -EINVAL;
	}
	iter->a3_body = off + hdr_len;
	iter->a3_end = off + hdr_len + val_len;
	n86 = count_children(bpp, iter->a3_body, iter->a3_end, 0x86);
	if (n86 < 0) {
		IPA_LOGP(SIPA, LERROR, "malformed '86' TLV in SequenceOf86!\n");
		return -EINVAL;
	}

	iter->count = 3 + (unsigned int)n88 + (iter->a2_off ? 1 : 0) + 1 + (unsigned int)n86;
	iter->state = SEG_ISCR;
	return 0;
}

/* Slice of the child TLV at iter->pos (tag validated by init); advances pos. */
static void child_slice(struct ipa_bpp_segment_iter *iter, struct ipa_buf *view)
{
	uint16_t tag;
	size_t val_len;
	size_t hdr_len = ipa_tlv_hdr(iter->bpp + iter->pos, iter->bpp_len - iter->pos, &tag, &val_len);

	ipa_buf_assign(view, iter->bpp + iter->pos, hdr_len + val_len);
	iter->pos += hdr_len + val_len;
}

int ipa_bpp_segment_iter_next(struct ipa_bpp_segment_iter *iter, struct ipa_buf *view)
{
	switch (iter->state) {
	case SEG_ISCR:
		ipa_buf_assign(view, iter->bpp, iter->seg0_len);
		iter->state = SEG_FIRST87;
		IPA_LOGP(SIPA, LDEBUG, "encoded InitialiseSecureChannelRequest segment:\n");
		ipa_buf_hexdump_multiline(view, 32, 1, SIPA, LDEBUG);
		break;
	case SEG_FIRST87:
		ipa_buf_assign(view, iter->bpp + iter->a0_off, iter->a0_seg_len);
		iter->state = SEG_HDR88;
		IPA_LOGP(SIPA, LDEBUG, "encoded FirstSequenceOf87 segment:\n");
		ipa_buf_hexdump_multiline(view, 32, 1, SIPA, LDEBUG);
		break;
	case SEG_HDR88:
		ipa_buf_assign(view, iter->bpp + iter->a1_off, iter->a1_body - iter->a1_off);
		iter->pos = iter->a1_body;
		iter->sub_idx = 0;
		iter->state = iter->pos < iter->a1_end ? SEG_EACH88 : (iter->a2_off ? SEG_SECOND87 : SEG_HDR86);
		IPA_LOGP(SIPA, LDEBUG, "encoded tag and length field of SequenceOf88 segment: %s\n",
			 ipa_buf_hexdump(view));
		break;
	case SEG_EACH88:
		child_slice(iter, view);
		IPA_LOGP(SIPA, LDEBUG, "encoded '88' TLV segment %u:\n", iter->sub_idx);
		ipa_buf_hexdump_multiline(view, 32, 1, SIPA, LDEBUG);
		iter->sub_idx++;
		if (iter->pos >= iter->a1_end)
			iter->state = iter->a2_off ? SEG_SECOND87 : SEG_HDR86;
		break;
	case SEG_SECOND87:
		ipa_buf_assign(view, iter->bpp + iter->a2_off, iter->a2_seg_len);
		iter->state = SEG_HDR86;
		IPA_LOGP(SIPA, LDEBUG, "encoded SecondSequenceOf87 segment:\n");
		ipa_buf_hexdump_multiline(view, 32, 1, SIPA, LDEBUG);
		break;
	case SEG_HDR86:
		ipa_buf_assign(view, iter->bpp + iter->a3_off, iter->a3_body - iter->a3_off);
		iter->pos = iter->a3_body;
		iter->sub_idx = 0;
		iter->state = iter->pos < iter->a3_end ? SEG_EACH86 : SEG_DONE;
		IPA_LOGP(SIPA, LDEBUG, "encoded tag and length field of SequenceOf86 segment: %s\n",
			 ipa_buf_hexdump(view));
		break;
	case SEG_EACH86:
		child_slice(iter, view);
		IPA_LOGP(SIPA, LDEBUG, "encoded '86' TLV segment %u:\n", iter->sub_idx);
		ipa_buf_hexdump_multiline(view, 32, 1, SIPA, LDEBUG);
		iter->sub_idx++;
		if (iter->pos >= iter->a3_end)
			iter->state = SEG_DONE;
		break;
	case SEG_DONE:
	default:
		return 0;
	}

	return 1;
}
