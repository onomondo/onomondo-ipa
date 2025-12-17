/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#define IPA_LEN_EUICC_CHLG 16	/* bytes */
#define IPA_LEN_SERV_CHLG 16	/* bytes */
#define IPA_LEN_EID 16		/* bytes */

/* This is the initial buffer size. The ASN.1 encoder will automatically re-alloc more memory if needed. */
#define IPA_LEN_ASN1_ENCODER_BUF 5120	/* bytes */

/* This is the initial buffer size. The eUICC interface will automatically re-alloc more memory if needed. */
#define IPA_LEN_EUICC_BUF 256 /* bytes */

/* This is the initial buffer size. The ASN.1 printer will automatically re-alloc more memory if needed. */
#define IPA_LEN_ASN1_PRINTER_BUF 10240	/* bytes */
