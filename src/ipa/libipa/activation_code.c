/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, 4.1: Activation Code
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "utils.h"
#include "activation_code.h"
#include <onomondo/ipa/log.h>

/*! Parse Activation code.
 *  \param[in] ac ASCII string that contains the activation code.
 *  \returns struct with parsed activation code on success, NULL on failure. */
struct ipa_activation_code *ipa_activation_code_parse(const char *ac)
{
	const char *item;
	char *item_end;
	char *item_buf;
	size_t item_len;
	size_t item_count = 0;

	struct ipa_activation_code *ac_decoded;

	/* No activation code */
	if (!ac)
		return NULL;

	/* Short activation code */
	if (strlen(ac) <= 4)
		return NULL;

	/* When the activation code is provided via a QR code it has to be
	 * prefixed with "LPA:", see also GSMA SGP.22, section 4.1 */
	if (memcmp("LPA:", ac, 4) == 0)
		ac += 4;

	/* Short activation code */
	if (strlen(ac) <= 4)
		return NULL;

	/* A valid activation code must start with format qualifier "1".
	 * SGP.22 4.1 also mentions an "Advanced Activation Code" with
	 * format qualifier "2", but seems not to specify it (yet). */
	if (memcmp("1$", ac, 2) != 0)
		return NULL;

	ac_decoded = IPA_ALLOC_ZERO(struct ipa_activation_code);

	item = ac;
	while (1) {
		item = strchr(item + 1, '$');
		if (!item)
			break;

		item_end = strchr(item + 1, '$');
		if (!item_end)
			item_len = strlen(item);
		else
			item_len = item_end - item;

		item_buf = IPA_ALLOC_N_ZERO(item_len + 1);
		memcpy(item_buf, item + 1, item_len - 1);
		item_buf[item_len - 1] = '\0';

		switch (item_count) {
		case 0:
			if (strlen(item_buf) >= 1)
				ac_decoded->sm_dp_plus_address = item_buf;
			else
				IPA_FREE(item_buf);
			break;
		case 1:
			ac_decoded->ac_token = item_buf;
			break;
		case 2:
			if (strlen(item_buf) > 0)
				ac_decoded->sm_dp_plus_oid = item_buf;
			else
				IPA_FREE(item_buf);
			break;
		case 3:
			if (item_buf[0] == '1')
				ac_decoded->confirmation_code_required = true;
			IPA_FREE(item_buf);
			break;
		case 4:
			if (strlen(item_buf) > 0)
				ac_decoded->ci_public_key_indicator = item_buf;
			else
				IPA_FREE(item_buf);
			break;
		case 5:
			if (strlen(item_buf) > 0)
				ac_decoded->delete_notification_for_device_change = item_buf;
			else
				IPA_FREE(item_buf);
			break;
		default:
			IPA_FREE(item_buf);

		}

		item_count++;
	};

	/* The SMDP+ address and the AC token are mandatory fields. In case we
	 * lack one of those fields, we will consider the whole AC as invalid.*/
	if (!ac_decoded->sm_dp_plus_address || !ac_decoded->ac_token) {
		ipa_activation_code_free(ac_decoded);
		ac_decoded = NULL;
	}

	return ac_decoded;
}

/*! Dump parsed Activation code to log.
 *  \param[in] ac_decoded pointer to struct with parsed activation code.
 *  \param[in] indent indentation level of the generated output.
 *  \param[in] log_subsys log subsystem to generate the output for.
 *  \param[in] log_level log level to generate the output for. */
void ipa_activation_code_dump(const struct ipa_activation_code *ac_decoded,
			      uint8_t indent, enum log_subsys log_subsys, enum log_level log_level)
{
	char indent_str[8];

	assert(indent < sizeof(indent_str));
	memset(indent_str, ' ', indent);
	indent_str[indent] = '\0';

	IPA_LOGP(log_subsys, log_level, "%sActivation Code: \n", indent_str);

	if (!ac_decoded) {
		IPA_LOGP(log_subsys, log_level, "%s (none)\n", indent_str);
		return;
	}

	IPA_LOGP(log_subsys, log_level, "%s SM-DP+Address: \"%s\"\n", indent_str, ac_decoded->sm_dp_plus_address);
	IPA_LOGP(log_subsys, log_level, "%s AC_Token: \"%s\"\n", indent_str, ac_decoded->ac_token);
	if (ac_decoded->sm_dp_plus_oid)
		IPA_LOGP(log_subsys, log_level, "%s SM-DP+ OID: \"%s\"\n", indent_str, ac_decoded->sm_dp_plus_oid);
	if (ac_decoded->confirmation_code_required)
		IPA_LOGP(log_subsys, log_level, "%s Confirmation Code Required Flag: (present)\n", indent_str);
	if (ac_decoded->ci_public_key_indicator)
		IPA_LOGP(log_subsys, log_level,
			 "%s CI Public Key indicator: \"%s\"\n", indent_str, ac_decoded->ci_public_key_indicator);
	if (ac_decoded->delete_notification_for_device_change)
		IPA_LOGP(log_subsys, log_level,
			 "%s Delete Notification for Device Change: \"%s\"\n",
			 indent_str, ac_decoded->delete_notification_for_device_change);

}

/*! Free parsed Activation code.
 *  \param[in] ac_decoded pointer to struct with parsed activation code. */
void ipa_activation_code_free(struct ipa_activation_code *ac_decoded)
{
	if (!ac_decoded)
		return;

	IPA_FREE(ac_decoded->sm_dp_plus_address);
	IPA_FREE(ac_decoded->ac_token);
	IPA_FREE(ac_decoded->sm_dp_plus_oid);
	IPA_FREE(ac_decoded->ci_public_key_indicator);
	IPA_FREE(ac_decoded->delete_notification_for_device_change);
	IPA_FREE(ac_decoded);
}
