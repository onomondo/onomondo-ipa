/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <PrepareDownloadResponse.h>
#include <onomondo/ipa/utils.h>
struct ipa_context;

struct ipa_esipa_get_bnd_prfle_pkg_req {
	const struct PrepareDownloadResponse *prep_dwnld_res;
};

/* The BoundProfilePackage is kept as a slice of the raw ESipa response — the
 * ES8+ segmentation works on the wire bytes (bpp_segments.h), so the response
 * is never decoded into an asn1c tree (which cost ~2x the wire size). */
struct ipa_esipa_get_bnd_prfle_pkg_res {
	struct ipa_buf *raw;  /* owned: the whole ESipa response */
	const uint8_t *bpp;   /* view into raw: the BoundProfilePackage TLV; NULL on error */
	size_t bpp_len;
	long get_bnd_prfle_pkg_err;
};

struct ipa_esipa_get_bnd_prfle_pkg_res *ipa_esipa_get_bnd_prfle_pkg(struct ipa_context *ctx,
								    const struct ipa_esipa_get_bnd_prfle_pkg_req *req);
void ipa_esipa_get_bnd_prfle_pkg_res_free(struct ipa_esipa_get_bnd_prfle_pkg_res *res);
