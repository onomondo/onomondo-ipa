/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/log.h>
#include <asn_application.h>
#include "utils.h"
#include "es10x.h"
#include "bpp_segments.h"

/* See also GSMA SGP.22, section  2.5.5 (bullet point 1) */
static struct ipa_buf *enc_init_sec_chan_req(const struct BoundProfilePackage *bpp,
					     const struct InitialiseSecureChannelRequest *init_sec_chan_req)
{
	struct ipa_buf *init_sec_chan_req_encoded = NULL;
	asn_enc_rval_t rc;

	/* "Tag and length fields of the BoundProfilePackage TLV..." */
	rc = der_encode(&asn_DEF_BoundProfilePackage, bpp, ipa_asn1c_consume_bytes_cb, &init_sec_chan_req_encoded);
	if (rc.encoded <= 0) {
		IPA_LOGP(SIPA, LDEBUG, "cannot re-encode BoundProfilePackage!\n");
		IPA_FREE(init_sec_chan_req_encoded);
		return NULL;
	}

	/* Pinch-off the value part of the TLV */
	init_sec_chan_req_encoded->len = ipa_parse_btlv_hdr(NULL, NULL, init_sec_chan_req_encoded);

	/* "...plus the initialiseSecureChannelRequest TLV */
	rc = der_encode(&asn_DEF_InitialiseSecureChannelRequest, init_sec_chan_req, ipa_asn1c_consume_bytes_cb,
			&init_sec_chan_req_encoded);

	if (rc.encoded <= 0) {
		IPA_LOGP(SIPA, LERROR, "cannot encode segment for InitialiseSecureChannelRequest!\n");
		IPA_FREE(init_sec_chan_req_encoded);
		return NULL;
	}

	IPA_LOGP(SIPA, LDEBUG, "encoded InitialiseSecureChannelRequest segment:\n");
	ipa_buf_hexdump_multiline(init_sec_chan_req_encoded, 32, 1, SIPA, LDEBUG);

	return init_sec_chan_req_encoded;
}

/* See also GSMA SGP.22, section  2.5.5 (bullet point 2) */
static struct ipa_buf *enc_first_seq_of_87(const struct BoundProfilePackage_FirstSequenceOf87 *first_seq_of_87)
{
	struct ipa_buf *first_seq_of_87_encoded = NULL;
	asn_enc_rval_t rc;

	/* "Tag and length fields of the FirstSequenceOf87 TLV" */
	rc = der_encode(&asn_DEF_BoundProfilePackage_FirstSequenceOf87, first_seq_of_87, ipa_asn1c_consume_bytes_cb,
			&first_seq_of_87_encoded);
	if (rc.encoded <= 0) {
		IPA_LOGP(SIPA, LERROR, "cannot encode segment for FirstSequenceOf87 (tlv header)!\n");
		IPA_FREE(first_seq_of_87_encoded);
		return NULL;
	}

	/* Pinch-off the value part of the TLV */
	first_seq_of_87_encoded->len = ipa_parse_btlv_hdr(NULL, NULL, first_seq_of_87_encoded);

	/* plus the first '87' TLV */
	if (first_seq_of_87->list.count < 1) {
		IPA_LOGP(SIPA, LERROR, "cannot encode segment for FirstSequenceOf87 (empty sequence)!\n");
		IPA_FREE(first_seq_of_87_encoded);
		return NULL;
	}
	if (first_seq_of_87->list.count > 1) {
		/* The spec explicitly states "first '87' TLV"! */
		IPA_LOGP(SIPA, LDEBUG, "ignoring excess items in the FirstSequenceOf87!\n");
	}

	rc = der_encode(&asn_DEF_BoundProfilePackage_87tlv, first_seq_of_87->list.array[0], ipa_asn1c_consume_bytes_cb,
			&first_seq_of_87_encoded);
	if (rc.encoded <= 0) {
		IPA_LOGP(SIPA, LERROR, "cannot encode segment for FirstSequenceOf87 (first 87 TLV)!\n");
		IPA_FREE(first_seq_of_87_encoded);
		return NULL;
	}

	IPA_LOGP(SIPA, LDEBUG, "encoded FirstSequenceOf87 segment:\n");
	ipa_buf_hexdump_multiline(first_seq_of_87_encoded, 32, 1, SIPA, LDEBUG);

	return first_seq_of_87_encoded;
}

/* See also GSMA SGP.22, section  2.5.5 (bullet point 3) */
static struct ipa_buf *enc_tag_and_len_of_sequenceOf88(const struct BoundProfilePackage_SequenceOf88 *seq_of_88)
{
	struct ipa_buf *seq_of_88_encoded = NULL;
	asn_enc_rval_t rc;

	/* "Tag and length fields of the first sequenceOf87 TLV" */
	rc = der_encode(&asn_DEF_BoundProfilePackage_SequenceOf88, seq_of_88, ipa_asn1c_consume_bytes_cb,
			&seq_of_88_encoded);
	if (rc.encoded <= 0) {
		IPA_LOGP(SIPA, LERROR, "cannot encode segment for tag and length field of SequenceOf88!\n");
		IPA_FREE(seq_of_88_encoded);
		return NULL;
	}

	/* Pinch-off the value part as we were only asked for tag and length fields */
	seq_of_88_encoded->len = ipa_parse_btlv_hdr(NULL, NULL, seq_of_88_encoded);

	IPA_LOGP(SIPA, LDEBUG, "encoded tag and length field of SequenceOf88 segment: %s\n",
		 ipa_buf_hexdump(seq_of_88_encoded));
	return seq_of_88_encoded;
}

/* See also GSMA SGP.22, section  2.5.5 (bullet point 4) */
static struct ipa_buf *enc_each_of_sequenceOf88(const BoundProfilePackage_88tlv_t * one_88tlv, unsigned int index)
{
	struct ipa_buf *one_88tlv_encoded = NULL;
	asn_enc_rval_t rc;

	/* "Each of the '88' TLVs" (= one per segment) */
	rc = der_encode(&asn_DEF_BoundProfilePackage_88tlv, one_88tlv, ipa_asn1c_consume_bytes_cb, &one_88tlv_encoded);
	if (rc.encoded <= 0) {
		IPA_LOGP(SIPA, LERROR, "cannot encode segment for '88' TLV %u!\n", index);
		IPA_FREE(one_88tlv_encoded);
		return NULL;
	}

	IPA_LOGP(SIPA, LDEBUG, "encoded '88' TLV segment %u:\n", index);
	ipa_buf_hexdump_multiline(one_88tlv_encoded, 32, 1, SIPA, LDEBUG);

	return one_88tlv_encoded;
}

/* See also GSMA SGP.22, section  2.5.5 (bullet point 5) */
static struct ipa_buf *enc_second_seq_of_87(const struct BoundProfilePackage_SecondSequenceOf87 *second_seq_of_87)
{
	struct ipa_buf *second_seq_of_87_encoded = NULL;
	asn_enc_rval_t rc;

	/* "Tag and length fields of the SecondSequenceOf87 TLV" */
	rc = der_encode(&asn_DEF_BoundProfilePackage_SecondSequenceOf87, second_seq_of_87, ipa_asn1c_consume_bytes_cb,
			&second_seq_of_87_encoded);
	if (rc.encoded <= 0) {
		IPA_LOGP(SIPA, LERROR, "cannot encode segment for SecondSequenceOf87 (tlv header)!\n");
		IPA_FREE(second_seq_of_87_encoded);
		return NULL;
	}

	/* Pinch-off the value part of the TLV */
	second_seq_of_87_encoded->len = ipa_parse_btlv_hdr(NULL, NULL, second_seq_of_87_encoded);

	/* plus the first '87' TLV */
	if (second_seq_of_87->list.count < 1) {
		IPA_LOGP(SIPA, LERROR, "cannot encode segment for SecondSequenceOf87 (empty sequence)!\n");
		IPA_FREE(second_seq_of_87_encoded);
		return NULL;
	}
	if (second_seq_of_87->list.count > 1) {
		/* The spec explicitly states "first '87' TLV"! */
		IPA_LOGP(SIPA, LDEBUG, "ignoring excess items in the SecondSequenceOf87!\n");
	}

	rc = der_encode(&asn_DEF_BoundProfilePackage_87tlv, second_seq_of_87->list.array[0], ipa_asn1c_consume_bytes_cb,
			&second_seq_of_87_encoded);
	if (rc.encoded <= 0) {
		IPA_LOGP(SIPA, LERROR, "cannot encode segment for SecondSequenceOf87 (first 87 TLV)!\n");
		IPA_FREE(second_seq_of_87_encoded);
		return NULL;
	}

	IPA_LOGP(SIPA, LDEBUG, "encoded SecondSequenceOf87 segment:\n");
	ipa_buf_hexdump_multiline(second_seq_of_87_encoded, 32, 1, SIPA, LDEBUG);

	return second_seq_of_87_encoded;
}

/* See also GSMA SGP.22, section  2.5.5 (bullet point 6) */
static struct ipa_buf *enc_tag_and_len_of_sequenceOf86(const struct BoundProfilePackage_SequenceOf86 *seq_of_86)
{
	struct ipa_buf *seq_of_86_encoded = NULL;
	asn_enc_rval_t rc;

	/* "Tag and length fields of the first sequenceOf87 TLV" */
	rc = der_encode(&asn_DEF_BoundProfilePackage_SequenceOf86, seq_of_86, ipa_asn1c_consume_bytes_cb,
			&seq_of_86_encoded);
	if (rc.encoded <= 0) {
		IPA_LOGP(SIPA, LERROR, "cannot encode segment for tag and length field of SequenceOf86!\n");
		IPA_FREE(seq_of_86_encoded);
		return NULL;
	}

	/* Pinch-off the value part as we were only asked for tag and length fields */
	seq_of_86_encoded->len = ipa_parse_btlv_hdr(NULL, NULL, seq_of_86_encoded);

	IPA_LOGP(SIPA, LDEBUG, "encoded tag and length field of SequenceOf86 segment: %s\n",
		 ipa_buf_hexdump(seq_of_86_encoded));

	return seq_of_86_encoded;
}

/* See also GSMA SGP.22, section  2.5.5 (bullet point 7) */
static struct ipa_buf *enc_each_of_sequenceOf86(const BoundProfilePackage_86tlv_t * one_86tlv, unsigned int index)
{
	struct ipa_buf *one_86tlv_encoded = NULL;
	asn_enc_rval_t rc;

	/* "Each of the '88' TLVs" (= one per segment) */
	rc = der_encode(&asn_DEF_BoundProfilePackage_86tlv, one_86tlv, ipa_asn1c_consume_bytes_cb, &one_86tlv_encoded);
	if (rc.encoded <= 0) {
		IPA_LOGP(SIPA, LERROR, "cannot encode segment for '86' TLV %u!\n", index);
		IPA_FREE(one_86tlv_encoded);
		return NULL;
	}

	IPA_LOGP(SIPA, LDEBUG, "encoded '86' TLV segment %u:\n", index);
	ipa_buf_hexdump_multiline(one_86tlv_encoded, 32, 1, SIPA, LDEBUG);

	return one_86tlv_encoded;
}

struct ipa_bpp_segments *ipa_bpp_segments_encode(const struct BoundProfilePackage *bpp)
{
	unsigned int i;
	struct ipa_bpp_segments *segments = NULL;
	struct ipa_buf *segment = NULL;
	size_t segment_count = 3 + bpp->sequenceOf88.list.count + 2 + bpp->sequenceOf86.list.count;

	segments = IPA_ALLOC_ZERO(struct ipa_bpp_segments);
	segments->segment = IPA_ALLOC_N(sizeof(*segments->segment) * segment_count);
	memset(segments->segment, 0, sizeof(*segments->segment) * segment_count);

	segment = enc_init_sec_chan_req(bpp, &bpp->initialiseSecureChannelRequest);
	if (!segment)
		goto error;
	segments->segment[segments->count] = segment;
	segments->count++;

	segment = enc_first_seq_of_87(&bpp->firstSequenceOf87);
	if (!segment)
		goto error;
	segments->segment[segments->count] = segment;
	segments->count++;

	segment = enc_tag_and_len_of_sequenceOf88(&bpp->sequenceOf88);
	if (!segment)
		goto error;
	segments->segment[segments->count] = segment;
	segments->count++;

	for (i = 0; i < bpp->sequenceOf88.list.count; i++) {
		segment = enc_each_of_sequenceOf88(bpp->sequenceOf88.list.array[i], i);
		if (!segment)
			goto error;
		segments->segment[segments->count] = segment;
		segments->count++;
	}

	/* Optional */
	if (bpp->secondSequenceOf87) {
		segment = enc_second_seq_of_87(bpp->secondSequenceOf87);
		if (!segment)
			goto error;
		segments->segment[segments->count] = segment;
		segments->count++;
	}

	segment = enc_tag_and_len_of_sequenceOf86(&bpp->sequenceOf86);
	if (!segment)
		goto error;
	segments->segment[segments->count] = segment;
	segments->count++;

	for (i = 0; i < bpp->sequenceOf86.list.count; i++) {
		segment = enc_each_of_sequenceOf86(bpp->sequenceOf86.list.array[i], i);
		if (!segment)
			goto error;
		segments->segment[segments->count] = segment;
		segments->count++;
	}

	return segments;
error:
	ipa_bpp_segments_free(segments);
	return NULL;
}

void ipa_bpp_segments_free(struct ipa_bpp_segments *segments)
{
	unsigned int i;

	if (!segments)
		return;

	for (i = 0; i < segments->count; i++)
		IPA_FREE(segments->segment[i]);
	IPA_FREE(segments->segment);

	IPA_FREE(segments);
}
