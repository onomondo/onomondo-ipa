/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 *
 * See also: GSMA SGP.22, section 3.1.3.2: Sub-procedure Profile Download and Installation – Download Confirmation
 */

/* TODO: fix spec reference, see github issue #5 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/log.h>
#include "context.h"
#include "utils.h"
#include "es10b_prep_dwnld.h"
#include "esipa_get_bnd_prfle_pkg.h"
#include "proc_prfle_dwnld.h"

/*! Perform Sub-procedure Profile Download and Installation – Download Confirmation.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] pars pointer to struct that holds the procedure parameters.
 *  \returns pointer newly allocated struct with procedure result, NULL on error. */
struct ipa_esipa_get_bnd_prfle_pkg_res *ipa_proc_prfle_dwnlod(struct ipa_context *ctx,
							      const struct ipa_proc_prfle_dwnlod_pars *pars)
{
	struct ipa_es10b_prep_dwnld_req prep_dwnld_req = { 0 };
	struct ipa_es10b_prep_dwnld_res *prep_dwnld_res = NULL;
	struct ipa_esipa_get_bnd_prfle_pkg_req get_bnd_prfle_pkg_req = { 0 };
	struct ipa_esipa_get_bnd_prfle_pkg_res *get_bnd_prfle_pkg_res = NULL;

	prep_dwnld_req.req.smdpSigned2 = pars->auth_clnt_ok_dpe->smdpSigned2;
	prep_dwnld_req.req.smdpSignature2 = pars->auth_clnt_ok_dpe->smdpSignature2;
	prep_dwnld_req.req.smdpSignature2.size =
	    ipa_strip_tlv_envelope(prep_dwnld_req.req.smdpSignature2.buf, prep_dwnld_req.req.smdpSignature2.size,
				   0x5f37);
	prep_dwnld_req.req.hashCc = pars->auth_clnt_ok_dpe->hashCc;
	prep_dwnld_req.req.smdpCertificate = pars->auth_clnt_ok_dpe->smdpCertificate;

	prep_dwnld_res = ipa_es10b_prep_dwnld(ctx, &prep_dwnld_req);
	if (!prep_dwnld_res)
		goto error;

	/* The request may still have failed but we do not have to take any action on this since we forward the
	 * result as a whole to the eIM. In case of failure it is the responsibility of the eIM to look at error
	 * codes and to react accordingly. */
	get_bnd_prfle_pkg_req.prep_dwnld_res = prep_dwnld_res->res;
	get_bnd_prfle_pkg_res = ipa_esipa_get_bnd_prfle_pkg(ctx, &get_bnd_prfle_pkg_req);
	if (!get_bnd_prfle_pkg_res)
		goto error;
	else if (get_bnd_prfle_pkg_res->get_bnd_prfle_pkg_err)
		goto error;
	else if (!get_bnd_prfle_pkg_res->get_bnd_prfle_pkg_ok)
		goto error;

	/* In case of error it is the responsibility of the caller to call the Common Cancel Session procedure.
	 * In case of success, the caller should ask the user for consent before continuing with the profile
	 * installation. */

	ipa_es10b_prep_dwnld_res_free(prep_dwnld_res);
	return get_bnd_prfle_pkg_res;
error:
	ipa_es10b_prep_dwnld_res_free(prep_dwnld_res);
	ipa_esipa_get_bnd_prfle_pkg_res_free(get_bnd_prfle_pkg_res);
	return NULL;
}
