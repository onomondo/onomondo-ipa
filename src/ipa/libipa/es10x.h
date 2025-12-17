/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/log.h>
struct asn_TYPE_descriptor_s;

#define IPA_LOGP_ES10X(func, level, fmt, args...) \
	IPA_LOGP(SES10X, level, "%s: " fmt, func, ## args)

#define IPA_ES10X_VERSION_LEN 3

void *ipa_es10x_res_dec(const struct asn_TYPE_descriptor_s *td, const struct ipa_buf *es10x_res_encoded,
			const char *function_name);
struct ipa_buf *ipa_es10x_req_enc(const struct asn_TYPE_descriptor_s *td, const void *es10x_req_decoded,
				  const char *function_name);

/*! A helper macro to free the basic contents of an ES10x response. This macro is intended to be used from within the
 *  concrete implementation of an ES10x function. It only frees the common contents and the struct itsself. In case
 *  there are other additional fields, the caller must free those first before calling this macro.
 *  \param[in] res pointer struct that holds the ES10x response. */
#define IPA_ES10X_RES_FREE(td, res) ({		\
	if (!(res)) \
		return; \
	ASN_STRUCT_FREE(td, (res)->res); \
	IPA_FREE((res)); \
})
