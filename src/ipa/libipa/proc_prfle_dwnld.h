#pragma once

#include <AuthenticateClientOkDPEsipa.h>

struct ipa_context;
struct ipa_esipa_get_bnd_prfle_pkg_res;

struct ipa_proc_prfle_dwnlod_pars {
	const AuthenticateClientOkDPEsipa_t *auth_clnt_ok_dpe;
};

struct ipa_esipa_get_bnd_prfle_pkg_res *ipa_proc_prfle_dwnlod(struct ipa_context *ctx,
							      const struct ipa_proc_prfle_dwnlod_pars *pars);
