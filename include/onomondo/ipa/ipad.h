/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdbool.h>

#define IPA_LEN_FQDN 255
#define IPA_LEN_TAC 4
#define IPA_LEN_ALLOWED_CA 20
#define IPE_LEN_EIM_ID 256

struct ipa_context;
struct ipa_buf;

/* (deprecated, see github issue #5) */
typedef bool (*ipa_prfle_inst_consent_cb)(char *sm_dp_plus_address, char *ac_token);

enum ipa_poll_rc {
	/*! The API user shall call ipa_poll() again immediately.
	 *  (there may be still eIM packages waiting to be executed). */
	IPA_POLL_AGAIN = 0,

	/*! The APU user may call ipa_poll() less frequently.
	 *  (there are no further eIM packages waiting) */
	IPA_POLL_AGAIN_LATER = 1,

	/*! An eIM package contained an operation (e.g. switch to another eUICC profile) that may cause a temporary loss
	 *  of the IP connectivity. The API user shall call ipa_poll() again as soon as the IP connectivity has
	 *  been resettled. */
	IPA_POLL_AGAIN_WHEN_ONLINE = 2,

	/*! Communication with the eUICC was not possible. The caller shall call ipa_popp() again when connectivity to
	 *  the eUICC has been recovered. */
	IPA_POLL_CHECK_SCARD = -1000,

	/*! Communication with the eIM was not possible. The caller shall call ipa_popp() again when connectivity to
	 *  the eIM has been recovered. */
	IPA_POLL_CHECK_HTTP = -2000,
};

/*! IPAd Configuration */
struct ipa_config {
	/*! preferred eimId (optional. When set to NULL, the first eIM config item in the EimConfigurationData list is
	 *  used.) */
	char *preferred_eim_id;

	/*! current TAC (This struct member may be updated at any time after context creation.) */
	uint8_t tac[IPA_LEN_TAC];

	/*! The caller may specify a path to a CA bundle file. The string is passed to ipa_http_init() on
	 *  initialization (see also http.h and http.c) */
	const char *eim_cabundle;

	/*! The caller may choose to disable SSL in a test environment to simplify debugging. */
	bool eim_disable_ssl;

	/*! The caller may choose to disable SSL certificate verification in a test environment to simplify debugging. */
	bool eim_disable_ssl_verif;

	/*! Configure the number of retries to apply in case a request (HTTP) to the eIM fails */
	unsigned int esipa_req_retries;

	/*! When a profile rollback is performed an optional refresh flag can be set. (See also SGP.32, section 5.9.16)
	 *  In case the IoT eUICC emulation is enabled (iot_euicc_emu_enabled), then this flag also plays a role when
	 *  profiles are disabled or enabled. (See also SGP.22, section 5.7.16 and section 5.7.17) */
	bool refresh_flag;

	/*! ID number of the cardreader that interfaces the eUICC */
	unsigned int reader_num;

	/*! Number of the logical channel that is used to communicate with the ISD-R */
	uint8_t euicc_channel;

	/*! Enable IoT eUICC emulation.
	 *  This IPAd also supports the use of consumer eUICCs, which have a slightly different interface. When the
	 *  IoT eUICC emulation is enabled, the IPAd will adapt the interface on ES10x function level so that the
	 *  consumer eUICC appears as an IoT eUICC on procedure level. */
	bool iot_euicc_emu_enabled;

	/*! (deprecated, see github issue #5) Consent to profile installation.
	 *  SGP.32 requires to prompt the user to consent to a profile installation. The API user may pass a callback
	 *  function here to handle the consent request. In case no callback function is provided onomondo-eim will
	 *  automatically consent to any profile installation. */
	ipa_prfle_inst_consent_cb prfle_inst_consent_cb;
};

struct ipa_context *ipa_new_ctx(struct ipa_config *cfg, struct ipa_buf *nvstate);
int ipa_init(struct ipa_context *ctx);
int eim_init(struct ipa_context *ctx);
int ipa_add_init_eim_cfg(struct ipa_context *ctx, struct ipa_buf *cfg);
int ipa_euicc_mem_rst(struct ipa_context *ctx, bool operatnl_profiles, bool test_profiles, bool default_smdp_addr,
		      bool eim_cfg_data, bool auto_enable_cfg);
int ipa_poll(struct ipa_context *ctx);
void ipa_close(struct ipa_context *ctx);
struct ipa_buf *ipa_free_ctx(struct ipa_context *ctx);
