/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 * 
 * See also: GSMA SGP.32, 5.9.4: Function (ES10b): AddInitialEim
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/log.h>
#include "context.h"
#include "utils.h"
#include "euicc.h"
#include "es10x.h"
#include "es10b_add_init_eim.h"

static const struct num_str_map error_code_strings[] = {
	{ AddInitialEimResponse__addInitialEimError_insufficientMemory, "insufficientMemory" },
	{ AddInitialEimResponse__addInitialEimError_unsignedEimConfigDisallowed, "unsignedEimConfigDisallowed" },
	{ AddInitialEimResponse__addInitialEimError_ciPKUnknown, "ciPKUnknown" },
	{ AddInitialEimResponse__addInitialEimError_invalidAssociationToken, "invalidAssociationToken" },
	{ AddInitialEimResponse__addInitialEimError_counterValueOutOfRange, "counterValueOutOfRange" },
	{ AddInitialEimResponse__addInitialEimError_undefinedError, "undefinedError" },
	{ 0, NULL }
};

static int dec_add_init_eim_res(struct ipa_es10b_add_init_eim_res *res, const struct ipa_buf *es10b_res)
{
	struct AddInitialEimResponse *asn = NULL;

	asn = ipa_es10x_res_dec(&asn_DEF_AddInitialEimResponse, es10b_res, "AddInitialEim");
	if (!asn)
		return -EINVAL;

	switch (asn->present) {
	case AddInitialEimResponse_PR_addInitialEimOk:
		/* When we see this list, we can be sure that the configuration was accepted. */
		break;
	case AddInitialEimResponse_PR_addInitialEimError:
		res->add_init_eim_err = asn->choice.addInitialEimError;
		IPA_LOGP_ES10X("AddInitialEim", LERROR, "function failed with error code %ld=%s!\n",
			       res->add_init_eim_err, ipa_str_from_num(error_code_strings, res->add_init_eim_err,
								       "(unknown)"));
		break;
	default:
		IPA_LOGP_ES10X("AddInitialEim", LERROR, "unexpected response content!\n");
		res->add_init_eim_err = -1;
	}

	res->res = asn;
	return 0;
}

static struct ipa_es10b_add_init_eim_res *add_init_eim(struct ipa_context *ctx,
						       const struct ipa_es10b_add_init_eim_req *req)
{
	struct ipa_buf *es10b_req = NULL;
	struct ipa_buf *es10b_res = NULL;
	struct ipa_es10b_add_init_eim_res *res = IPA_ALLOC_ZERO(struct ipa_es10b_add_init_eim_res);
	int rc;

	es10b_req = ipa_es10x_req_enc(&asn_DEF_AddInitialEimRequest, &req->req, "AddInitialEim");
	if (!es10b_req) {
		IPA_LOGP_ES10X("AddInitialEim", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	es10b_res = ipa_euicc_transceive_es10x(ctx, es10b_req);
	if (!es10b_res) {
		IPA_LOGP_ES10X("AddInitialEim", LERROR, "no ES10b response\n");
		goto error;
	}

	rc = dec_add_init_eim_res(res, es10b_res);
	if (rc < 0)
		goto error;

	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	return res;
error:
	IPA_FREE(es10b_req);
	IPA_FREE(es10b_res);
	ipa_es10b_add_init_eim_res_free(res);
	return NULL;
}

/* This function is only relevant in case the IoT eUICC emulation is enabled. It checks the presented eIM configuration
 * for completeness and adds missing values in the same way a native IoT eUICC would do. */
int complete_eim_cfg(struct ipa_context *ctx, struct EimConfigurationData *eim_cfg)
{
	/* The eimFqdn is not strictly mandatory, but it is necessary to reach the eIM at all. This makes eimFqdn a
	 * mandatory IE in this implementation. */
	if (!eim_cfg->eimFqdn) {
		IPA_LOGP_ES10X("AddInitialEim", LERROR, "eimFqdn is missing from eimConfigurationData!\n");
		return -1;
	}

	/* Mandatory, See also SGP.32, section 5.9.4 */
	if (!eim_cfg->counterValue) {
		IPA_LOGP_ES10X("AddInitialEim", LERROR, "counterValue is missing from eimConfigurationData!\n");
		return -1;
	}

	/* Mandatory, See also SGP.32, section 5.9.4 (but not used/ignored by the IoT eUICC emulation) */
	if (!eim_cfg->eimPublicKeyData && !eim_cfg->trustedPublicKeyDataTls)
		IPA_LOGP_ES10X("AddInitialEim", LINFO,
			       "eimPublicKeyData/trustedPublicKeyDataTls is missing from eimConfigurationData!\n");

	/* Calculate a new associationToken if requested */
	if (eim_cfg->associationToken && *eim_cfg->associationToken == -1) {
		ctx->nvstate.iot_euicc_emu.association_token_counter++;
		*eim_cfg->associationToken = ctx->nvstate.iot_euicc_emu.association_token_counter;
	}

	/* TODO: This validation function is not complete yet. It currently barely covers the parameters that IoT
	 * eUICC emulation needs. However, it might make sense to do more validation of the input data to ensure
	 * only spec compliant eIM configuration files can be loaded. (see also SGP.32, section 5.9.4) */
	return 0;
}

struct AddInitialEimRequest *complete_eim_cfg_list(struct ipa_context *ctx, const struct AddInitialEimRequest *req)
{
	struct AddInitialEimRequest *req_dup;
	unsigned int i;
	int rc;

	req_dup = ipa_asn1c_dup(&asn_DEF_AddInitialEimRequest, req);

	for (i = 0; i < req_dup->eimConfigurationDataList.list.count; i++) {
		rc = complete_eim_cfg(ctx, req_dup->eimConfigurationDataList.list.array[i]);
		if (rc < 0)
			goto error;
	}

	return req_dup;
error:
	ASN_STRUCT_FREE(asn_DEF_AddInitialEimRequest, req_dup);
	return NULL;
}

struct AddInitialEimResponse *generate_add_init_eim_response(struct ipa_context *ctx,
							     const struct AddInitialEimRequest *req)
{
	struct AddInitialEimResponse *res = IPA_ALLOC_ZERO(struct AddInitialEimResponse);
	struct AddInitialEimResponse__addInitialEimOk__Member *add_init_eim_item;
	unsigned int i;

	assert(res);
	res->present = AddInitialEimResponse_PR_addInitialEimOk;
	for (i = 0; i < req->eimConfigurationDataList.list.count; i++) {
		add_init_eim_item = IPA_ALLOC_ZERO(struct AddInitialEimResponse__addInitialEimOk__Member);
		assert(add_init_eim_item);
		if (req->eimConfigurationDataList.list.array[i]->associationToken) {
			add_init_eim_item->present = AddInitialEimResponse__addInitialEimOk__Member_PR_associationToken;
			add_init_eim_item->choice.associationToken =
			    *req->eimConfigurationDataList.list.array[i]->associationToken;
		} else {
			add_init_eim_item->present = AddInitialEimResponse__addInitialEimOk__Member_PR_addOk;
			add_init_eim_item->choice.addOk = 0;
		}
		ASN_SEQUENCE_ADD(&res->choice.addInitialEimOk.list, add_init_eim_item);
	}

	return res;
}

struct AddInitialEimResponse *generate_add_init_eim_response_err(void)
{
	struct AddInitialEimResponse *res = IPA_ALLOC_ZERO(struct AddInitialEimResponse);

	assert(res);
	res->present = AddInitialEimResponse_PR_addInitialEimError;
	res->choice.addInitialEimError = AddInitialEimResponse__addInitialEimError_undefinedError;
	return res;
}

static struct ipa_es10b_add_init_eim_res *add_init_eim_iot_emu(struct ipa_context *ctx,
							       const struct ipa_es10b_add_init_eim_req *req)
{
	struct ipa_buf *eim_cfg_new = NULL;
	struct AddInitialEimRequest *req_cfg_new_decoded = NULL;
	struct ipa_es10b_add_init_eim_res *res = IPA_ALLOC_ZERO(struct ipa_es10b_add_init_eim_res);

	IPA_LOGP_ES10X("AddInitialEim", LINFO,
		       "IoT eUICC emulation active, pretending to query eUICC to set eIM configuration...\n");

	req_cfg_new_decoded = complete_eim_cfg_list(ctx, &req->req);
	if (!req_cfg_new_decoded) {
		IPA_LOGP_ES10X("AddInitialEim", LERROR, "unable to complete ES10b request\n");
		goto error;
	}
	eim_cfg_new = ipa_es10x_req_enc(&asn_DEF_AddInitialEimRequest, req_cfg_new_decoded, "AddInitialEim");
	if (!eim_cfg_new) {
		IPA_LOGP_ES10X("AddInitialEim", LERROR, "unable to encode ES10b request\n");
		goto error;
	}

	/* AddInitialEimRequest and GetEimConfigurationDataResponse are identical. This means we can cast
	 * AddInitialEimRequest encoded ASN.1 data to GetEimConfigurationDataResponse */
	eim_cfg_new->data[1] = 0x55;

	/* Replace the current eIM configuration with the new eIM configuration. If there is already an eIM
	 * configuration in place it will be deleted and replaced with the new eIM configuration. This
	 * behaviour contradicts the behaviour of a real IoT eUICC, which would reject any new eIM configuration
	 * in that case. However, since this is an emulation and there is no reasonable security around that
	 * eIM configuration anyway, we decided to allow unconditional overwriting an existing eIM configuration. */
	IPA_FREE(ctx->nvstate.iot_euicc_emu.eim_cfg_ber);
	ctx->nvstate.iot_euicc_emu.eim_cfg_ber = eim_cfg_new;
	eim_cfg_new = NULL;	/* Ownership is now at ctx->nvstate.iot_euicc_emu.eim_cfg_ber */
	IPA_LOGP_ES10X("AddInitialEim", LINFO, "done, eIM configuration stored in memory.\n");

	res->res = generate_add_init_eim_response(ctx, req_cfg_new_decoded);
	IPA_FREE(eim_cfg_new);
	ASN_STRUCT_FREE(asn_DEF_AddInitialEimRequest, req_cfg_new_decoded);
	return res;
error:
	res->res = generate_add_init_eim_response_err();
	res->add_init_eim_err = res->res->choice.addInitialEimError;
	IPA_FREE(eim_cfg_new);
	ASN_STRUCT_FREE(asn_DEF_AddInitialEimRequest, req_cfg_new_decoded);
	return res;
}

/*! Function (ES10b): AddInitialEim.
 *  \param[inout] ctx pointer to ipa_context.
 *  \param[in] req pointer to struct that holds the function parameters.
 *  \returns pointer newly allocated struct with function result, NULL on error. */
struct ipa_es10b_add_init_eim_res *ipa_es10b_add_init_eim(struct ipa_context *ctx,
							  const struct ipa_es10b_add_init_eim_req *req)
{
	if (ctx->cfg->iot_euicc_emu_enabled)
		return add_init_eim_iot_emu(ctx, req);
	else
		return add_init_eim(ctx, req);
}

/*! Free results of function (ES10b): AddInitialEim.
 *  \param[in] res pointer to function result. */
void ipa_es10b_add_init_eim_res_free(struct ipa_es10b_add_init_eim_res *res)
{
	IPA_ES10X_RES_FREE(asn_DEF_AddInitialEimResponse, res);
}
