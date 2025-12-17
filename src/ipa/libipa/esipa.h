/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <EsipaMessageFromEimToIpa.h>
#include <EsipaMessageFromIpaToEim.h>
#include <SGP32-RetrieveNotificationsListResponse.h>
#include <RetrieveNotificationsListResponse.h>
struct ipa_buf;

#define IPA_LOGP_ESIPA(func, level, fmt, args...) \
	IPA_LOGP(SESIPA, level, "%s: " fmt, func, ## args)

#define IPA_ESIPA_URL_MAXLEN 1024

char *ipa_esipa_get_eim_url(struct ipa_context *ctx);
struct EsipaMessageFromEimToIpa *ipa_esipa_msg_to_ipa_dec(const struct ipa_buf *msg_to_ipa_encoded,
							  const char *function_name,
							  enum EsipaMessageFromEimToIpa_PR epected_res_type);
struct ipa_buf *ipa_esipa_msg_to_eim_enc(const struct EsipaMessageFromIpaToEim *msg_to_eim, const char *function_name);
struct ipa_buf *ipa_esipa_req(struct ipa_context *ctx, const struct ipa_buf *esipa_req, const char *function_name);
void ipa_esipa_close(struct ipa_context *ctx);

/*! A helper macro to free the basic contents of an ESIPA response. This macro is intended to be used from within the
 *  concrete implementation of an ESIPA function. It only frees the common contents and the struct itsself. In case
 *  there are other additional fields, the caller must free those first before calling this macro.
 *  \param[in] res pointer struct that holds the ESIPA response. */
#define IPA_ESIPA_RES_FREE(res) ({ \
	if (!(res)) \
		return; \
	ASN_STRUCT_FREE(asn_DEF_EsipaMessageFromEimToIpa, (res)->msg_to_ipa); \
	IPA_FREE((res)); \
})
