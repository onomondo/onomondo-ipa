/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

/* SGP.32 section 6.1: the User-Agent SHALL be gsma-rsp-ipad (or
 * gsma-rsp-ipae) and MAY carry additional information after a semicolon;
 * X-Admin-Protocol SHALL be v2.1.0 (interoperability with SGP.22). */
#define IPA_HTTP_USER_AGENT "gsma-rsp-ipad; Onomondo-IPAd/1.0.0"
#define IPA_HTTP_X_ADMIN_PROTOCOL "gsma/rsp/v2.1.0"
#define IPA_HTTP_CONTENT_TYPE "application/x-gsma-rsp-asn1"
