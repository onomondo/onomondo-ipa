#pragma once

#include <BoundProfilePackage.h>

struct ipa_context;

struct ipa_proc_prfle_inst_pars {
	const struct BoundProfilePackage *bound_profile_package;
};

int ipa_proc_prfle_inst(struct ipa_context *ctx, const struct ipa_proc_prfle_inst_pars *pars);
