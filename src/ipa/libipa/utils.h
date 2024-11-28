#pragma once

#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/log.h>
#include <errno.h>
#include <stddef.h>
#include <stdbool.h>

struct asn_TYPE_descriptor_s;

/*! A mapping between human-readable string and numeric value, when forming arrays of this struct, the last entry
 *  must have member str set to NULL. */
struct num_str_map {
	uint32_t num;
	const char *str;
};

const char *ipa_str_from_num(const struct num_str_map *map, long num, const char *def);
int ipa_asn1c_consume_bytes_cb(const void *buffer, size_t size, void *priv);
void ipa_asn1c_dump(const struct asn_TYPE_descriptor_s *td, const void *struct_ptr, uint8_t indent,
		    enum log_subsys log_subsys, enum log_level log_level);
int ipa_cmp_case_insensitive(const char *str1, const char *str2, size_t len);
bool ipa_tag_in_taglist(uint16_t tag, const struct ipa_buf *tag_list);
size_t ipa_parse_btlv_hdr(size_t *len, uint16_t *tag, struct ipa_buf *buf);
int ipa_strip_tlv_envelope(uint8_t *data, size_t data_len, uint16_t envelope_tag);
void *ipa_asn1c_dup(const struct asn_TYPE_descriptor_s *td, const void *struct_ptr);

/* \! Compare an ASN.1 string object to another ASN.1 string object.
 *  \param[in] asn1_obj1 pointer to first asn1c generated string object to compare.
 *  \param[in] asn1_obj2 pointer to second asn1c generated string object to compare.
 *  \returns true on match, false on mismatch. */
#define IPA_ASN_STR_CMP(asn1_obj1, asn1_obj2) ({ \
	bool __rc = true; \
	if (!(asn1_obj1) || !(asn1_obj2)) \
		__rc = false; \
	else if ((asn1_obj1)->size != (asn1_obj2)->size) \
		__rc = false; \
	else if (memcmp((asn1_obj1)->buf, (asn1_obj2)->buf, (asn1_obj1)->size)) \
		__rc = false; \
	__rc; \
})

/* \! Compare an ASN.1 string object to buffer.
 *  \param[in] asn1_obj pointer to asn1c generated string object to compare.
 *  \param[in] buf_ptr pointer to buffer with data to compare against.
 *  \param[in] buf_len length of the data.
 *  \returns true on match, false on mismatch. */
#define IPA_ASN_STR_CMP_BUF(asn1_obj, buf_ptr, buf_len) ({ \
	bool __rc = true; \
	if (!(asn1_obj) || !(buf_ptr)) \
		__rc = false; \
	else if ((asn1_obj)->size != buf_len) \
		__rc = false; \
	else if (memcmp((asn1_obj)->buf, (buf_ptr), buf_len)) \
		__rc = false; \
	__rc; \
})

/* \! Compare an ASN.1 string object to buffer (case insensitive).
 *  \param[in] asn1_obj pointer to asn1c generated string object to compare.
 *  \param[in] buf_ptr pointer to buffer with data to compare against.
 *  \param[in] buf_len length of the data.
 *  \returns true on match, false on mismatch. */
#define IPA_ASN_STR_CMP_BUF_I(asn1_obj, buf_ptr, buf_len) ({ \
	bool __rc = true; \
	if (!(asn1_obj) || !(buf_ptr)) \
		__rc = false; \
	else if ((asn1_obj)->size != buf_len) \
		__rc = false; \
	else if (ipa_cmp_case_insensitive((char*)(asn1_obj)->buf, (char*)(buf_ptr), buf_len)) \
		__rc = false; \
	__rc; \
})

/* \! Copy an ASN.1 string object into a dynamically allocated IPA_BUF. This macro is used in situations where the ASN.1
 *    specification defines a string type with arbitrary length. Then the target buffer where the data is copied to will
 *    be implemented as a pointer of type uint8_t.
 *  \param[in] asn1_obj pointer to asn1c generated string object to read from.
 *  \returns dynamically allocated IPA_BUF with contents of asn1_obj. */
#define IPA_BUF_FROM_ASN(asn1_obj) ({ \
	struct ipa_buf *__ipa_buf; \
	assert(asn1_obj); \
	__ipa_buf = ipa_buf_alloc((asn1_obj)->size); \
	assert(__ipa_buf); \
	memcpy(__ipa_buf->data, (asn1_obj)->buf, (asn1_obj)->size); \
	__ipa_buf->len = (asn1_obj)->size; \
	__ipa_buf; \
})

/* \! Copy an ASN.1 string object into a dynamically allocated char array. This macro is used in situations where the
 *    ASN.1 specification defines a printable string of an arbitrary length. Then the target buffer where the data is
 *    copied to will be implemented as a pointer of type char.
 *  \param[in] asn1_obj pointer to asn1c generated string object to read from.
 *  \returns null terminated char array with contents of asn1_obj. */
#define IPA_STR_FROM_ASN(asn1_obj) ({ \
	char *__str; \
	assert(asn1_obj); \
	__str = IPA_ALLOC_N((asn1_obj)->size + 1); \
	assert(__str); \
	memcpy(__str, (asn1_obj)->buf, (asn1_obj)->size); \
	__str[(asn1_obj)->size] = '\0';	\
	__str; \
})

/* \! Copy an ASN.1 string object into a statically allocated buffer. This macro is used in situations where the ASN.1
 *    specification defines a fixed length string type. Then the target buffer will be implemented as a fixed length
 *    statically allocated buffer. (To check the buffer sizes, this macro uses sizeof(dest_buf) and the size member of
 *    the ASN.1 object. The target buffer must be bigger or equal in size as the size of the ASN.1 string.)
 *  \param[out] dest_buf destination buffer where the data is copied to.
 *  \param[in] asn1_obj pointer to asn1c generated string object to read from.
 *  \returns 0 on success, -ENOMEM on failure. */
#define IPA_COPY_ASN_BUF(dest_buf, asn1_obj) ({ \
	int __rc = -ENOMEM; \
	memset(dest_buf, 0, sizeof(dest_buf)); \
	if (sizeof(dest_buf) >= (asn1_obj)->size) { \
		memcpy(dest_buf, (asn1_obj)->buf, (asn1_obj)->size); \
		__rc = 0; \
	} \
	__rc; \
})

/* \! Assign a pointer to a buffer and a length to an ASN.1 string object This macro is used to fill an empty ASN.1
 *    structure that is used for encoding later. The data is not copied, only pointers are assigned.
 *    \param[out] asn1_obj pointer to asn1c generated string object to equip.
 *    \param[in] buf_ptr pointer to buffer with data.
 *    \param[in] buf_len length of the data. */
#define IPA_ASSIGN_BUF_TO_ASN(asn1_obj, buf_ptr, buf_len) ({ \
	asn1_obj.buf = buf_ptr; \
	asn1_obj.size = buf_len; \
})

/* \! Same as IPA_ASSIGN_BUF_TO_ASN, but for null terminated strings. Here the length is determined using strlen().
 *    \param[out] asn1_obj pointer to asn1c generated string object to equip.
 *    \param[in] buf_ptr pointer to buffer with null terminate string. */
#define IPA_ASSIGN_STR_TO_ASN(asn1_obj, str_ptr) ({ \
	asn1_obj.buf = (uint8_t*)str_ptr; \
	asn1_obj.size = strlen(str_ptr); \
})

/* \! Assign a the buf pointer and len from an ipa_buf to an to an ASN.1 string object. This macro is used to fill
 *    an empty ASN.1 structure that is used for encoding later. The data is not copied, only pointers are assigned.
 *    \param[out] asn1_obj pointer to asn1c generated string object to equip.
 *    \param[in] ipa_buf_ptr pointer to ipa_buf with data. */
#define IPA_ASSIGN_IPA_BUF_TO_ASN(asn1_obj, ipa_buf_ptr) ({ \
	asn1_obj.buf = (ipa_buf_ptr)->data; \
	asn1_obj.size = (ipa_buf_ptr)->len; \
})

/* \! Copy the contents of an ipa_buf to an ASN.1 string object. The target buffer in the ASN.1 string object is
 *    allocated by this macro. The data is actually copied, so the source ipa_buf may be freed when copying is done.
 *    If this macro is used in situations where lists have to be populated with ASN.1 string objects (SEQUENCE OF),
 *    the caller is expected to allocate the ASN.1 string object and equip its buf and size member using this macro.
 *    \param[out] asn1_obj pointer to asn1c generated string object where the data should be copied to.
 *    \param[in] ipa_buf pointer to ipa_buf object to copy from. */
#define IPA_COPY_IPA_BUF_TO_ASN(asn1_obj, ipa_buf) ({ \
	(asn1_obj)->buf = IPA_ALLOC_N((ipa_buf)->len);	\
	assert((asn1_obj)->buf); \
	memcpy((asn1_obj)->buf, (ipa_buf)->data, (ipa_buf)->len); \
	(asn1_obj)->size = (ipa_buf)->len; \
})

/* \! Copy the contents of an ASN.1 string object to an existing ipa_buf. The data is actually copied, so the source
 *    ASN.1 object may be freed when copying is done. It should also be noted that the target ipa_buf object must
 *    be initialized and equipped with sufficient buffer space.
 *    \param[out] ipa_buf pointer to the target ipa_buf where the data should be copied to.
 *    \param[in] asn1_obj pointer to asn1c generated string object to copy from. */
#define IPA_COPY_ASN_TO_IPA_BUF(ipa_buf, asn1_obj) ({	\
	int __rc = -ENOMEM; \
	if ((ipa_buf)->data_len >= (asn1_obj)->size) { \
		memcpy((ipa_buf)->data, (asn1_obj)->buf, (asn1_obj)->size);	\
		(ipa_buf)->len = (asn1_obj)->size;				\
		__rc = 0; \
	} \
	__rc; \
})

/* \! Free an allocated SEQUENCE OF ASN.1 string objects. This macro is used to get rid of a list of ASN.1 string
 *    objects that the caller has allocated before. The macro loops through that list, frees the buffers and ASN.1
 *    string objects and eventually the list pointer array also. This macro must not be used on ASN.1 structs that
 *    the decoder has allocated.
 *    \param[inout] asn1_list_obj pointer to the ASN.1 list object to be freed. */
#define IPA_FREE_ASN_SEQUENCE_OF_STRINGS(asn1_list_obj) ({ \
	int __i; \
	for (__i = 0; __i < asn1_list_obj.list.count; __i++) { \
		IPA_FREE(asn1_list_obj.list.array[__i]->buf); \
		IPA_FREE(asn1_list_obj.list.array[__i]); \
	} \
	IPA_FREE(asn1_list_obj.list.array); \
})
