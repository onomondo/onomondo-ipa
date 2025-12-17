/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <string.h>
#include <stdbool.h>
#include <onomondo/ipa/log.h>

/* SGP.22, section 4.1 */
struct ipa_activation_code {
	/* Mandatory */
	char *sm_dp_plus_address;
	char *ac_token;

	/* Optional */
	char *sm_dp_plus_oid;
	bool confirmation_code_required;
	char *ci_public_key_indicator;
	char *delete_notification_for_device_change;
};

struct ipa_activation_code *ipa_activation_code_parse(const char *ac);
void ipa_activation_code_dump(const struct ipa_activation_code *ac_decoded,
			      uint8_t indent, enum log_subsys log_subsys, enum log_level log_level);
void ipa_activation_code_free(struct ipa_activation_code *ac_decoded);
