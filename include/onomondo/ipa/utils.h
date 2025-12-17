/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "mem.h"
#include "log.h"

/*! Get the size of an array in elements.
 *  \param[in] array array reference. */
#define IPA_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/*! Allocate memory for an object, ensure that the allocation was successful and that the memory is initialized.
 *  \param[in] obj description of the object to allocated (struct).
 *  \returns dynamically allocated memory of the object size. */
#define IPA_ALLOC_ZERO(obj) ({ \
	obj *__ptr; \
	__ptr = IPA_ALLOC(obj); \
	assert(__ptr); \
	memset(__ptr, 0, sizeof(*__ptr)); \
	__ptr; \
})

/*! Allocate N bytes of memory, ensure that the allocation was successful and that the memory is initialized.
 *  \param[in] n number of bytes to allocate.
 *  \returns N bytes of dynamically allocated memory. */
#define IPA_ALLOC_N_ZERO(n) ({ \
	void *__ptr; \
	__ptr = IPA_ALLOC_N(n); \
	assert(__ptr); \
	memset(__ptr, 0, n); \
	__ptr; \
})

char *ipa_hexdump(const uint8_t *data, size_t len);

struct ipa_buf {
	/*! pointer to allocated memory */
	uint8_t *data;

	/*! actual length of the allocated memory. This parameter is set when
	 * the buffer is allocated and must not be modified by the API user. */
	size_t data_len;

	/*! length of useful data. This value may be modified by the API user,
	 * it is used to tell how many bytes with useful data are stored. */
	size_t len;
};

/*! Generate a hexdump string from an ipa_buf object.
 *  \param[in] buf pointer to ipa_buf object.
 *  \returns pointer to generated human readable string. */
static inline char *ipa_buf_hexdump(const struct ipa_buf *buf)
{
	if (!buf)
		return "(null)";
	return ipa_hexdump(buf->data, buf->len);
}

void ipa_hexdump_multiline(const uint8_t *data, size_t len, size_t width, uint8_t indent, enum log_subsys log_subsys,
			   enum log_level log_level);
void ipa_buf_hexdump_multiline(const struct ipa_buf *buf, size_t width, uint8_t indent, enum log_subsys log_subsys,
			       enum log_level log_level);

/*! Allocate a new ipa_buf object.
 *  \param[in] len number of bytes to allocate inside ipa_buf.
 *  \returns pointer to newly allocated ipa_buf object. */
static inline struct ipa_buf *ipa_buf_alloc(size_t len)
{
	struct ipa_buf *buf = IPA_ALLOC_N(sizeof(*buf) + len);
	assert(buf);

	memset(buf, 0, sizeof(*buf));
	buf->data = (uint8_t *) buf + sizeof(*buf);
	buf->data_len = len;

	/* A newly allocated ipa_buf naturally has 0 bytes of
	 * useful data in it. */
	buf->len = 0;

	return buf;
}

/*! Reallocate/Resize an existing ipa_buf object.
 *  \param[in] len new number of bytes to allocate inside ipa_buf.
 *  \returns pointer to newly allocated ipa_buf object. */
static inline struct ipa_buf *ipa_buf_realloc(struct ipa_buf *buf, size_t len)
{
	buf = IPA_REALLOC(buf, sizeof(*buf) + len);
	assert(buf);

	buf->data = (uint8_t *) buf + sizeof(*buf);
	buf->data_len = len;
	memset(buf->data + buf->len, 0, len - buf->len);

	return buf;
}

/*! Create a statically allocated ipa_buf object (do not use with ipa_buf_free or ipa_buf_realloc!).
 *  \param[in] name symbol name of the ipa_buf.
 *  \returns size size of the ipa_buf object. */
#define IPA_BUF_STATIC(name, size) \
	uint8_t __name_buf[size]; \
	struct ipa_buf name = { __name_buf, size, 0 };

/*! Allocate a new ipa_buf object and initialize it with data.
 *  \param[in] len number of bytes to allocate inside ipa_buf.
 *  \param[in] data to copy into the newly allocated ipa_buf.
 *  \returns pointer to newly allocated ipa_buf object. */
static inline struct ipa_buf *ipa_buf_alloc_data(size_t len, uint8_t *data)
{
	struct ipa_buf *buf = ipa_buf_alloc(len);
	assert(buf);

	buf->len = len;
	memcpy(buf->data, data, len);

	return buf;
}

/*! Duplicate (exact copy) from another ipa_buf object.
 *  \param[in] buf ipa_buf object to duplicate.
 *  \returns pointer to newly allocated ipa_buf object. */
static inline struct ipa_buf *ipa_buf_dup(const struct ipa_buf *buf)
{
	struct ipa_buf *buf_dup = ipa_buf_alloc(buf->data_len);
	memcpy(buf_dup->data, buf->data, buf->data_len);
	buf_dup->len = buf->len;
	return buf_dup;
}

/*! Allocate a new ipa_buf and copy the data from another ipa_buf object.
 *  \param[in] buf ipa_buf object to copy from.
 *  \returns pointer to newly allocated ipa_buf object. */
static inline struct ipa_buf *ipa_buf_copy(const struct ipa_buf *buf)
{
	struct ipa_buf *buf_dup = ipa_buf_alloc(buf->len);
	memcpy(buf_dup->data, buf->data, buf->len);
	buf_dup->len = buf->len;
	return buf_dup;
}

/*! Allocate a new ipa_buf and copy from user provided memory.
 *  \param[in] in user provided memory to copy.
 *  \param[in] len amount of bytes to copy from user provided memory.
 *  \returns pointer to newly allocated ss_buf object. */
static inline struct ipa_buf *ipa_buf_alloc_and_cpy(const uint8_t *in, size_t len)
{
	struct ipa_buf *buf = ipa_buf_alloc(len);
	memcpy(buf->data, in, len);
	buf->len = len;
	return buf;
}

/*! Allocate a new ipa_buf and copy from user provided memory.
 *  \param[in] buf ipa_buf where the data should be copied (appended) to.
 *  \param[in] in user provided memory to copy.
 *  \param[in] len amount of bytes to copy from user provided memory. */
static inline void ipa_buf_cpy(struct ipa_buf *buf, const uint8_t *in, size_t len)
{
	assert(buf->len + len <= buf->data_len);
	memcpy(buf->data + buf->len, in, len);
	buf->len += len;
}

/*! Assign data from a different location to an uninitialized ipa_buf struct.
 *  \param[in] buf uninitialized ipa_buf (possibly statically allocated).
 *  \param[in] data user provided memory to assign to the ipa_buf.
 *  \param[in] len length of the user provided memory to assign. */
static inline void ipa_buf_assign(struct ipa_buf *buf, const uint8_t *data, size_t len)
{
	/*! The purpose of this function is to provide an easy way to assign
	 *  already existing memory locations to an ipa_buf struct. The result
	 *  is a valid ipa_buf struct, however it must not be freed using
	 *  ipa_buf_free(). */
	memset(buf, 0, sizeof(*buf));
	buf->data = (uint8_t *) data;
	buf->data_len = len;
	buf->len = len;
}

/*! Deserialize ipa_buf from a buffer (data may come from a file or similar).
 *  \param[in] data user provided memory with serialized ipa_buf.
 *  \param[in] len length of the user provided memory that contains the serialied ipa_buf. */
static inline struct ipa_buf *ipa_buf_deserialize(uint8_t *data, size_t len)
{
	/*! An ipa_buf is serialized by writing its header to a file and append its data section directly after. Since
	 *  ipa_buf_alloc already allocates an ipa_buf object this way no extra effort has to be taken. It is
	 *  sufficient to pass the pointer to the ipa_buf object to memcpy and use sizeof(*buf) + buf->data_len as
	 *  length. */

	struct ipa_buf *buf_serialized;
	struct ipa_buf *buf;

	/* This will give us an almost working ipa_buf (the data pointer will be stale) */
	buf_serialized = (struct ipa_buf *)data;

	/* First we allocate a new buffer from the data in the serialzed buffer. We cannot trust the data pointer since
	 * this serialzed buffer may have come from a different process on a different machine, so we must calculate
	 * the beginning of the data ourselves. We also must be suere to copy the complete memory. */
	buf = ipa_buf_alloc_data(buf_serialized->data_len, (uint8_t *) buf_serialized + sizeof(*buf_serialized));

	/* The original buffer may not have utilized all the available memory, so we restore the length. */
	buf->len = buf_serialized->len;

	return buf;
}

/*! Free an ipa_buf object.
 *  \param[in] pointer to ipa_buf object to free. */
static inline void ipa_buf_free(struct ipa_buf *buf)
{
	IPA_FREE(buf);
}

size_t ipa_binary_from_hexstr(uint8_t *binary, size_t binary_len, const char *hexstr);
