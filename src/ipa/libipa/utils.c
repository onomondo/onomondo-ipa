/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/log.h>
#include <asn_application.h>
#include "utils.h"
#include "length.h"

/* \! Lookup a numeric value in a num to string map and return the corresponding string.
 *  \param[in] map pointer num to str map.
 *  \param[in] num numeric value of the string to look up.
 *  \param[in] def default string to return in case the numeric value is not found.
 *  \returns found string from map, default string in case of no match. */
const char *ipa_str_from_num(const struct num_str_map *map, long num, const char *def)
{
	do {
		if (map->num == num)
			return map->str;
		map++;
	} while (map->str != NULL);

	return def;
}

/*! Generate a hexdump string from the input data.
 *  \param[in] data pointer to binary data.
 *  \param[in] len length of binary data.
 *  \returns pointer to generated human readable string. */
/* ring of 2: no call site evaluates more than two hexdumps per statement */
#define IPA_HEXDUMP_MAX 2
#define IPA_HEXDUMP_BUFSIZE 256
char *ipa_hexdump(const uint8_t *data, size_t len)
{
	static char out[IPA_HEXDUMP_MAX][IPA_HEXDUMP_BUFSIZE];
	static uint8_t idx = 0;
	char *out_ptr;
	size_t i;

	idx++;
	idx = idx % IPA_HEXDUMP_MAX;
	out_ptr = out[idx];

	if (!data)
		return ("(null)");

	for (i = 0; i < len; i++) {
		sprintf(out_ptr, "%02X", data[i]);
		out_ptr += 2;

		/* put three dots and exit early in case we are running out of
		 * space */
		if (i > IPA_HEXDUMP_BUFSIZE / 2 - 4) {
			sprintf(out_ptr, "...");
			return out[idx];
		}
	}

	*out_ptr = '\0';
	return out[idx];
}

/*! Log binary data as multiple lines of hex strings (useful for large amounts of data).
 *  \param[in] data pointer to binary data.
 *  \param[in] len length of binary data.
 *  \param[in] indent indentation level of the generated output.
 *  \param[in] log_subsys log subsystem to generate the output for.
 *  \param[in] log_level log level to generate the output for. */
void ipa_hexdump_multiline(const uint8_t *data, size_t len, size_t width, uint8_t indent, enum log_subsys log_subsys,
			   enum log_level log_level)
{
	size_t l;
	size_t bsize;
	char indent_str[8];

	assert(indent < sizeof(indent_str));
	memset(indent_str, ' ', indent);
	indent_str[indent] = '\0';

	if (!data) {
		IPA_LOGP(log_subsys, log_level, "%s(none)\n", indent_str);
		return;
	}

	l = 0;
	do {
		bsize = width;
		if (len < l + bsize)
			bsize = len - l;
		IPA_LOGP(log_subsys, log_level, "%s%s\n", indent_str, ipa_hexdump(data + l, bsize));
		l += bsize;
	} while (len - l > 0);
}

/*! Log binary data contents of an ipa_buf as multiple lines of hex strings (useful for large amounts of data).
 *  \param[in] buf pointer to an ipa_buf that contains the binary data.
 *  \param[in] indent indentation level of the generated output.
 *  \param[in] log_subsys log subsystem to generate the output for.
 *  \param[in] log_level log level to generate the output for. */
void ipa_buf_hexdump_multiline(const struct ipa_buf *buf, size_t width, uint8_t indent, enum log_subsys log_subsys,
			       enum log_level log_level)
{
	char indent_str[8];

	assert(indent < sizeof(indent_str));
	memset(indent_str, ' ', indent);
	indent_str[indent] = '\0';

	if (!buf) {
		IPA_LOGP(log_subsys, log_level, "%s(none)\n", indent_str);
		return;
	}

	ipa_hexdump_multiline(buf->data, buf->len, width, indent, log_subsys, log_level);
}

/*! Generate a hexdump string from the input data.
 *  \param[in] buffer pointer to chunk with encoded data.
 *  \param[in] size length of encoded data chunk.
 *  \param[out] priv pointer to a pointer (may be NULL) that points to a caller provided ipa_buf that stores the output.
 *  \returns 0 on success, -ENOMEM on error. */
int ipa_asn1c_consume_bytes_cb(const void *buffer, size_t size, void *priv)
{
	struct ipa_buf **buf_encoded_ptr = priv;
	struct ipa_buf *buf_encoded = *buf_encoded_ptr;
	size_t realloc_size;

	assert(priv);
	assert(buffer);

	/* In case the caller didn't provide an initial buffer, we allocate one */
	if (!buf_encoded) {
		buf_encoded = ipa_buf_alloc(IPA_LEN_ASN1_ENCODER_BUF);
		assert(buf_encoded);
		*buf_encoded_ptr = buf_encoded;
	}

	/* Check whether we still have enough space to store the encoding
	 * results. */
	if (buf_encoded->data_len < buf_encoded->len + size) {
		realloc_size = ((buf_encoded->len + size) / IPA_LEN_ASN1_ENCODER_BUF + 1) * IPA_LEN_ASN1_ENCODER_BUF;
		IPA_LOGP(SIPA, LDEBUG,
			 "ASN.1 encoder buffer exhausted, reallocating more memory (have: %zu bytes, required: %zu bytes, will allocate: %zu bytes)\n",
			 buf_encoded->data_len, buf_encoded->len + size, realloc_size);
		buf_encoded = ipa_buf_realloc(buf_encoded, realloc_size);
		assert(buf_encoded);
		*buf_encoded_ptr = buf_encoded;
	}

	ipa_buf_cpy(buf_encoded, buffer, size);

	return 0;
}

/* Capture buffer for ipa_asn1c_capture_hdr_cb: large enough for any BER
 * tag + length field we produce (tag <= 2 bytes, length <= 0x84 + 4 bytes). */
struct ipa_asn1c_hdr_capture {
	uint8_t data[16];
	size_t len;
};

static int ipa_asn1c_capture_hdr_cb(const void *buffer, size_t size, void *priv)
{
	struct ipa_asn1c_hdr_capture *cap = priv;
	size_t room = sizeof(cap->data) - cap->len;

	if (size > room)
		size = room;
	memcpy(cap->data + cap->len, buffer, size);
	cap->len += size;

	/* Abort the encoder once the header is safely captured -- this is
	 * what keeps the whole (potentially multi-kilobyte) value part from
	 * ever being materialized. */
	if (cap->len >= sizeof(cap->data))
		return -1;
	return 0;
}

/*! Encode only the tag and length field of a DER TLV.
 *  \param[in] td asn1c type descriptor of the structure to encode.
 *  \param[in] struct_ptr pointer to the structure to encode.
 *  \returns pointer to a newly allocated ipa_buf holding exactly the TLV
 *  header, NULL on error.
 *
 *  A DER encoder must emit the tag and length field first, so the encoding
 *  is aborted right after the header bytes are captured. This avoids
 *  buffering the complete TLV (the value part of a BoundProfilePackage is
 *  several kilobytes) when only its header is needed. */
struct ipa_buf *ipa_asn1c_enc_tlv_hdr(const struct asn_TYPE_descriptor_s *td, const void *struct_ptr)
{
	struct ipa_asn1c_hdr_capture cap = { 0 };
	struct ipa_buf cap_buf;
	size_t hdr_len;

	/* rc is intentionally ignored: aborting from the callback makes
	 * der_encode() report failure even though the header was emitted. */
	der_encode(td, struct_ptr, ipa_asn1c_capture_hdr_cb, &cap);
	if (cap.len == 0) {
		IPA_LOGP(SIPA, LERROR, "cannot encode TLV header!\n");
		return NULL;
	}

	cap_buf.data = cap.data;
	cap_buf.data_len = sizeof(cap.data);
	cap_buf.len = cap.len;
	hdr_len = ipa_parse_btlv_hdr(NULL, NULL, &cap_buf);
	if (hdr_len == 0 || hdr_len > cap.len) {
		IPA_LOGP(SIPA, LERROR, "cannot parse encoded TLV header!\n");
		return NULL;
	}

	return ipa_buf_alloc_data(hdr_len, cap.data);
}

/*! DER-encode a structure into an ipa_buf that is sized upfront (appending).
 *  \param[in] td asn1c type descriptor of the structure to encode.
 *  \param[in] struct_ptr pointer to the structure to encode.
 *  \param[inout] buf_encoded_ptr pointer to a pointer to the destination
 *  ipa_buf; *buf_encoded_ptr may be NULL, then a new buffer is allocated.
 *  \returns 0 on success, -EINVAL on error.
 *
 *  A size-only encoder pass determines the exact space needed, so the
 *  destination buffer is (re)allocated once instead of growing through
 *  repeated reallocations (each of which briefly holds old + new buffer). */
int ipa_asn1c_der_encode(const struct asn_TYPE_descriptor_s *td, const void *struct_ptr,
			 struct ipa_buf **buf_encoded_ptr)
{
	struct ipa_buf *buf_encoded = *buf_encoded_ptr;
	asn_enc_rval_t rc;
	size_t used;

	/* Size-only pass (no consume callback). */
	rc = der_encode(td, struct_ptr, NULL, NULL);
	if (rc.encoded <= 0)
		return -EINVAL;

	used = buf_encoded ? buf_encoded->len : 0;
	if (!buf_encoded) {
		buf_encoded = ipa_buf_alloc(rc.encoded);
		assert(buf_encoded);
		*buf_encoded_ptr = buf_encoded;
	} else if (buf_encoded->data_len < used + rc.encoded) {
		buf_encoded = ipa_buf_realloc(buf_encoded, used + rc.encoded);
		assert(buf_encoded);
		*buf_encoded_ptr = buf_encoded;
	}

	rc = der_encode(td, struct_ptr, ipa_asn1c_consume_bytes_cb, buf_encoded_ptr);
	if (rc.encoded <= 0)
		return -EINVAL;

	return 0;
}

struct ipa_asn1c_dump_buf {
	char *printbuf;
	char *printbuf_ptr;
	size_t printbuf_size;
};

#ifdef SHOW_ASN_OUTPUT
static int ipa_asn1c_dump_consume(const void *buffer, size_t size, void *app_key)
{

	struct ipa_asn1c_dump_buf *buf = app_key;
	size_t realloc_size;
	size_t current_size;

	if ((buf->printbuf_ptr - buf->printbuf) + size >= buf->printbuf_size) {
		realloc_size =
		    (((buf->printbuf_ptr - buf->printbuf) + size) / IPA_LEN_ASN1_PRINTER_BUF +
		     1) * IPA_LEN_ASN1_PRINTER_BUF;
		IPA_LOGP(SMAIN, LDEBUG,
			 "ASN.1 print buffer exhausted - allocating more space for up to %zu characters!\n",
			 realloc_size);

		current_size = buf->printbuf_ptr - buf->printbuf;
		buf->printbuf = IPA_REALLOC(buf->printbuf, realloc_size);
		buf->printbuf_ptr = buf->printbuf + current_size;
		buf->printbuf_size = realloc_size;
		memset(buf->printbuf_ptr, 0, realloc_size - current_size);
	}

	memcpy(buf->printbuf_ptr, buffer, size);
	buf->printbuf_ptr[size] = '\0';

	buf->printbuf_ptr += size;

	return 0;
}
#endif

/*! dump contents of a decoded ASN.1 structure.
 *  \param[in] td pointer to asn_TYPE_descriptor.
 *  \param[in] struct_ptr pointer to decoded ASN.1 struct.
 *  \param[in] indent indentation level of the generated output.
 *  \param[in] log_subsys log subsystem to generate the output for.
 *  \param[in] log_level log level to generate the output for. */
void ipa_asn1c_dump(const struct asn_TYPE_descriptor_s *td, const void *struct_ptr, uint8_t indent,
		    enum log_subsys log_subsys, enum log_level log_level)
{
#ifdef SHOW_ASN_OUTPUT
	struct ipa_asn1c_dump_buf buf = { 0 };
#endif
	char indent_str[8];
	assert(indent < sizeof(indent_str));
	memset(indent_str, ' ', indent);
	indent_str[indent] = '\0';

#ifdef SHOW_ASN_OUTPUT
	buf.printbuf = IPA_ALLOC_N_ZERO(IPA_LEN_ASN1_PRINTER_BUF);
	buf.printbuf_ptr = buf.printbuf;
	buf.printbuf_size = IPA_LEN_ASN1_PRINTER_BUF;
	td->op->print_struct(td, struct_ptr, 1, ipa_asn1c_dump_consume, &buf);

	char *token = strtok(buf.printbuf, "\n");

	while (token != NULL) {
		IPA_LOGP(log_subsys, log_level, "%s %s\n", indent_str, token);
		token = strtok(NULL, "\n");
	}

	IPA_FREE(buf.printbuf);
#else
	IPA_LOGP(log_subsys, log_level,
		 "%s (decoded ASN.1 output omitted, compile with -DSHOW_ASN_OUTPUT to display decoed ASN.1)\n",
		 indent_str);
#endif
}

/*! Compare two strings case insensitive.
 *  \param[in] str1 first string to compare.
 *  \param[in] str2 second string to compare.
 *  \param[in] len length up to which we compare the two strings.
 *  \returns 0 when both strings match, -1 when there is a difference. */
int ipa_cmp_case_insensitive(const char *str1, const char *str2, size_t len)
{
	size_t i;

	if (!str1)
		return -1;
	if (!str2)
		return -1;

	for (i = 0; i < len; i++) {
		if (toupper(str1[i]) != toupper(str2[i]))
			return -1;
	}

	return 0;
}

/*! Check whether a BTLV tag is contained in a tag list.
 *  \param[in] tag tag to search for.
 *  \param[in] tag_list ipa_buf that contains the tag list.
 *  \returns true when the tag is found in the list, false otherwise. */
bool ipa_tag_in_taglist(uint16_t tag, const struct ipa_buf *tag_list)
{
	uint8_t *tag_list_ptr;
	uint16_t tag_from_list;
	size_t tag_bytes_left;

	tag_list_ptr = tag_list->data;
	tag_bytes_left = tag_list->len;

	while (1) {
		if (tag_bytes_left >= 2 && (tag_list_ptr[0] & 0x1F) == 0x1F) {
			tag_from_list = tag_list_ptr[0] << 8;
			tag_from_list |= tag_list_ptr[1];
			tag_list_ptr += 2;
			tag_bytes_left -= 2;
		} else if (tag_bytes_left >= 1 && (tag_list_ptr[0] & 0x1F) != 0x1F) {
			tag_from_list = tag_list_ptr[0];
			tag_list_ptr++;
			tag_bytes_left--;
		} else
			return false;

		if (tag_from_list == tag)
			return true;
	}

	return false;
}

/* Returns the header length (offset to the value part), 0 on error. A valid
 * BER TLV header is never shorter than 2 bytes, so 0 is unambiguous. */
static size_t parse_btlv_hdr(size_t *len, uint16_t *tag, uint8_t *data, size_t data_len)
{
	uint8_t tag_len = 1;
	/* size_t, not uint16_t: the length field may carry up to 4 length
	 * bytes, and outer BoundProfilePackage TLVs can exceed 64 KiB. */
	size_t value_len = 0;
	uint8_t len_bytes;
	size_t skip_len = 0;
	unsigned int i;

	/* decode tag */
	if ((*data & 0x1f) == 0x1f)
		tag_len = 2;
	if (data_len < tag_len)
		return 0;
	if (tag && tag_len == 1) {
		*tag = *data;
	} else if (tag && tag_len == 2) {
		*tag = (*data) << 8;
		*tag |= *(data + 1);
	}
	data += tag_len;
	data_len -= tag_len;
	skip_len += tag_len;

	/* decode length */
	if (*data < 0x7f) {
		if (data_len < 1)
			return 0;
		value_len = *data;
		data++;
		data_len--;
		skip_len++;
	} else {
		len_bytes = *data & 0x7f;
		if (len_bytes == 0 || len_bytes > 4)
			return 0;
		if (data_len < 1)
			return 0;
		data++;
		data_len--;
		skip_len++;
		value_len = 0;
		for (i = 0; i < len_bytes; i++) {
			value_len <<= 8;
			if (data_len < 1)
				return 0;
			value_len |= *data;
			data++;
			data_len--;
			skip_len++;
		}
	}

	if (len)
		*len = value_len;

	return skip_len;
}

/*! Parse a BER TLV tag from an ipa_buf.
 *  \param[out] len length as specified in the TLV header (caller may pass NULL if not interested).
 *  \param[out] tag tag value from the TLV header (caller may pass NULL if not interested).
 *  \returns total length of the TLV header (offset to the beginning of the value part), 0 on error. */
size_t ipa_parse_btlv_hdr(size_t *len, uint16_t *tag, struct ipa_buf *buf)
{
	return parse_btlv_hdr(len, tag, buf->data, buf->len);
}

/*! Strip a TLV envelope (if it is present) from a buffer.
 *  \param[inout] buf ipa_buf that contains the data to be stripped
 *  \param[in] envelope_tag tag of the envelope (as a verification so we won't strip random data).
 *  \returns new length of the data. */
int ipa_strip_tlv_envelope(uint8_t *data, size_t data_len, uint16_t envelope_tag)
{
	size_t chop_bytes = 0;
	uint16_t tlv_tag = 0;

	chop_bytes = parse_btlv_hdr(NULL, &tlv_tag, data, data_len);

	/* The header is invalid, this indicates that this buffer has no TLV header, so the envelope we looking for
	 * is also not present. (parse_btlv_hdr returns 0 on error; the old < 0 check could never fire on the
	 * unsigned return type, and tlv_tag was read uninitialized in that case.) */
	if (chop_bytes == 0)
		return data_len;

	/* The header is valid, but the TLV tag does not match, so the envelope we
	 * looking for is not present either. */
	if (tlv_tag != envelope_tag)
		return data_len;

	/* The number of bytes to be chopped exceeds the data length, something can not be right, so we better
	 * do not touch the buffer */
	if (chop_bytes > data_len)
		return data_len;

	/* Chop bytes and return new data length */
	memmove(data, data + chop_bytes, data_len - chop_bytes);
	return data_len - chop_bytes;
}

static bool is_hex(char hex_digit)
{
	switch (tolower(hex_digit)) {
	case 'a':
	case 'b':
	case 'c':
	case 'd':
	case 'e':
	case 'f':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '0':
		return true;
	}

	return false;
}

/*! Convert a human readable hex string to its binary representation.
 *  \param[in] binary pointer to binary data.
 *  \param[in] binary_len length of binary data.
 *  \param[in] hexstr string with human readable representation.
 *  \returns number resulting bytes. */
size_t ipa_binary_from_hexstr(uint8_t *binary, size_t binary_len, const char *hexstr)
{
	unsigned int i;
	size_t hexstr_len;
	char hex_digit[3];
	unsigned int hex_digit_bin;
	size_t binary_count = 0;
	int rc;

	hexstr_len = strlen(hexstr);

	memset(binary, 0, binary_len);

	for (i = 0; i < hexstr_len / 2; i++) {
		hex_digit[0] = hexstr[0];
		hex_digit[1] = hexstr[1];
		hex_digit[2] = '\0';
		hexstr += 2;

		if (!is_hex(hex_digit[0]) || !is_hex(hex_digit[1]))
			hex_digit_bin = 0xff;
		else {
			rc = sscanf(hex_digit, "%02x", &hex_digit_bin);
			if (rc != 1)
				hex_digit_bin = 0xff;
		}

		binary[binary_count] = (uint8_t) hex_digit_bin & 0xff;
		binary_count++;

		if (binary_count >= binary_len)
			break;
	}

	return binary_count;
}

/*! Duplicate/Copy an existing decoded ASN.1 struct.
 *  \param[in] td pointer to asn_TYPE_descriptor.
 *  \param[in] struct_ptr pointer to decoded ASN.1 struct to be duplicated.
 *  \returns pointer duplicated ASN.1 struct, NULL on error. */
void *ipa_asn1c_dup(const struct asn_TYPE_descriptor_s *td, const void *struct_ptr)
{
	struct ipa_buf *buf_encoded = NULL;
	asn_enc_rval_t rc_enc;
	asn_dec_rval_t rc_dec;

	void *struct_ptr_dup = NULL;

	if (!struct_ptr)
		return NULL;

	rc_enc = der_encode(td, struct_ptr, ipa_asn1c_consume_bytes_cb, &buf_encoded);
	if (rc_enc.encoded <= 0) {
		IPA_FREE(buf_encoded);
		return NULL;
	}

	rc_dec = ber_decode(0, td, (void **)&struct_ptr_dup, buf_encoded->data, buf_encoded->len);
	if (rc_dec.code != RC_OK) {
		IPA_FREE(buf_encoded);
		ASN_STRUCT_FREE(*td, struct_ptr_dup);
		return NULL;
	}

	IPA_FREE(buf_encoded);
	return struct_ptr_dup;
}
