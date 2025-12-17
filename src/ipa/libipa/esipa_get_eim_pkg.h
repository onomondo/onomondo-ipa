/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <EsipaMessageFromEimToIpa.h>
#include <EuiccPackageRequest.h>
#include <IpaEuiccDataRequest.h>
#include <ProfileDownloadTriggerRequest.h>
struct ipa_context;

struct ipa_esipa_get_eim_pkg_res {
	struct EsipaMessageFromEimToIpa *msg_to_ipa;
	struct EuiccPackageRequest *euicc_package_request;
	struct IpaEuiccDataRequest *ipa_euicc_data_request;
	struct ProfileDownloadTriggerRequest *dwnld_trigger_request;
	long eim_pkg_err;
};

struct ipa_esipa_get_eim_pkg_res *ipa_esipa_get_eim_pkg(struct ipa_context *ctx, const uint8_t *eid);
void ipa_esipa_get_eim_pkg_free(struct ipa_esipa_get_eim_pkg_res *res);
