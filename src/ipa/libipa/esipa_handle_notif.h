/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <ProfileInstallationResult.h>
struct ipa_context;

struct ipa_esipa_handle_notif_req {
	const struct ProfileInstallationResult *profile_installation_result;
	const struct SGP32_PendingNotification *pending_notification;
	/*! The API user must populate either profile_installation_result or pending_notification, but not both at the
	 *  same time. */
};

int ipa_esipa_handle_notif(struct ipa_context *ctx, const struct ipa_esipa_handle_notif_req *req);
