/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <GetEimConfigurationDataResponse.h>
struct ipa_context;
struct ipa_buf;

/*! Struct to hold a single item of the EimConfigurationData list */
struct ipa_eim_cfg_data {
	char *eim_id;
	char *eim_fqdn;
	long *eim_id_type;
	long *counter_value;
	long *association_token;
	struct {
		struct ipa_buf *eim_public_key;
		struct ipa_buf *eim_certificate;
	} eim_public_key_data;
	struct {
		struct ipa_buf *trusted_eim_pk_tls;
		struct ipa_buf *trusted_certificate_tls;
	} trusted_public_key_data_tls;
	struct ipa_buf *eim_supported_protocol;
	struct ipa_buf *euicc_ci_pkid;
};

struct ipa_es10b_eim_cfg_data {
	struct GetEimConfigurationDataResponse *res;

	/* The GetEimConfigurationDataResponse contains a list of EimConfigurationData elements. To simplify the
	 * the access to the list items and their members, the list is automatically converted into an array of
	 * struct ipa_eim_cfg_data items (see above). In case the caller wants to take ownership of eim_cfg_data_list,
	 * he can do so by setting eim_cfg_data_list to NULL before calling ipa_es10b_get_eim_cfg_data_free. */
	struct ipa_eim_cfg_data **eim_cfg_data_list;
	long eim_cfg_data_list_count;
};

struct ipa_es10b_eim_cfg_data *ipa_es10b_get_eim_cfg_data(struct ipa_context *ctx);
void ipa_es10b_get_eim_cfg_data_free(struct ipa_es10b_eim_cfg_data *res);

struct EimConfigurationData *ipa_es10b_get_eim_cfg_data_filter(struct ipa_es10b_eim_cfg_data *res, char *eim_id);
