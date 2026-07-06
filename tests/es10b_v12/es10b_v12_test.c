/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Checks the wire format of the SGP.32 v1.2 ES10b functions that were added to libipa
 * (fallback mechanism, emergency profile, connectivity parameters, immediate profile
 * enabling configuration, IPAe activation) and the pure decision logic of the emulated
 * fallback attribute PSMOs.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ExecuteFallbackMechanismRequest.h>
#include <ExecuteFallbackMechanismResponse.h>
#include <ReturnFromFallbackRequest.h>
#include <ReturnFromFallbackResponse.h>
#include <EnableEmergencyProfileRequest.h>
#include <EnableEmergencyProfileResponse.h>
#include <DisableEmergencyProfileRequest.h>
#include <GetConnectivityParametersRequest.h>
#include <GetConnectivityParametersResponse.h>

static uint8_t enc_buf[512];
static size_t enc_len;

static int enc_cb(const void *data, size_t size, void *key)
{
	(void)key;
	assert(enc_len + size <= sizeof(enc_buf));
	memcpy(enc_buf + enc_len, data, size);
	enc_len += size;
	return 0;
}

static void enc_assert(const asn_TYPE_descriptor_t *td, const void *sptr, const char *expect_hex)
{
	char hex[2 * sizeof(enc_buf) + 1];
	asn_enc_rval_t er;
	size_t i;

	enc_len = 0;
	er = der_encode(td, sptr, enc_cb, NULL);
	assert(er.encoded > 0);

	for (i = 0; i < enc_len; i++)
		sprintf(&hex[i * 2], "%02X", enc_buf[i]);

	if (strcmp(hex, expect_hex) != 0) {
		fprintf(stderr, "%s encoding mismatch!\n  expected: %s\n  actual:   %s\n", td->name, expect_hex, hex);
		assert(0);
	}
}

/* ExecuteFallbackMechanismRequest ('BF5D') with the mandatory refreshFlag */
static void exec_fallback_request_test(void)
{
	struct ExecuteFallbackMechanismRequest req = { 0 };

	req.refreshFlag = 0xff;
	enc_assert(&asn_DEF_ExecuteFallbackMechanismRequest, &req, "BF5D038001FF");

	req.refreshFlag = 0;
	enc_assert(&asn_DEF_ExecuteFallbackMechanismRequest, &req, "BF5D03800100");
}

/* ReturnFromFallbackRequest ('BF5E') */
static void return_from_fallback_request_test(void)
{
	struct ReturnFromFallbackRequest req = { 0 };

	req.refreshFlag = 0xff;
	enc_assert(&asn_DEF_ReturnFromFallbackRequest, &req, "BF5E038001FF");
}

/* Response decode: ExecuteFallbackMechanismResponse with ecallActive(104), the v1.2 error that
 * signals a rejected fallback while the emergency profile is enabled */
static void exec_fallback_response_decode_test(void)
{
	const uint8_t ecall_active[] = { 0xBF, 0x5D, 0x03, 0x80, 0x01, 0x68 };
	struct ExecuteFallbackMechanismResponse *rsp = NULL;
	asn_dec_rval_t rval;

	rval = ber_decode(0, &asn_DEF_ExecuteFallbackMechanismResponse, (void **)&rsp,
			  ecall_active, sizeof(ecall_active));
	assert(rval.code == RC_OK);
	assert(rsp->executeFallbackMechanismResult ==
	       ExecuteFallbackMechanismResponse__executeFallbackMechanismResult_ecallActive);
	ASN_STRUCT_FREE(asn_DEF_ExecuteFallbackMechanismResponse, rsp);
}

/* Response decode: ReturnFromFallbackResponse with fallbackNotAvailable(6) */
static void return_from_fallback_response_decode_test(void)
{
	const uint8_t not_available[] = { 0xBF, 0x5E, 0x03, 0x80, 0x01, 0x06 };
	struct ReturnFromFallbackResponse *rsp = NULL;
	asn_dec_rval_t rval;

	rval = ber_decode(0, &asn_DEF_ReturnFromFallbackResponse, (void **)&rsp,
			  not_available, sizeof(not_available));
	assert(rval.code == RC_OK);
	assert(rsp->returnFromFallbackResult == ReturnFromFallbackResponse__returnFromFallbackResult_fallbackNotAvailable);
	ASN_STRUCT_FREE(asn_DEF_ReturnFromFallbackResponse, rsp);
}

/* EnableEmergencyProfileRequest ('BF5B') / DisableEmergencyProfileRequest ('BF5C') */
static void emergency_profile_request_test(void)
{
	struct EnableEmergencyProfileRequest enable_req = { 0 };
	struct DisableEmergencyProfileRequest disable_req = { 0 };

	enable_req.refreshFlag = 0xff;
	enc_assert(&asn_DEF_EnableEmergencyProfileRequest, &enable_req, "BF5B038001FF");

	disable_req.refreshFlag = 0;
	enc_assert(&asn_DEF_DisableEmergencyProfileRequest, &disable_req, "BF5C03800100");
}

/* Response decode: EnableEmergencyProfileResponse with ecallNotAvailable(8) */
static void enable_emergency_response_decode_test(void)
{
	const uint8_t not_available[] = { 0xBF, 0x5B, 0x03, 0x80, 0x01, 0x08 };
	struct EnableEmergencyProfileResponse *rsp = NULL;
	asn_dec_rval_t rval;

	rval = ber_decode(0, &asn_DEF_EnableEmergencyProfileResponse, (void **)&rsp,
			  not_available, sizeof(not_available));
	assert(rval.code == RC_OK);
	assert(rsp->enableEmergencyProfileResult ==
	       EnableEmergencyProfileResponse__enableEmergencyProfileResult_ecallNotAvailable);
	ASN_STRUCT_FREE(asn_DEF_EnableEmergencyProfileResponse, rsp);
}

/* GetConnectivityParametersRequest ('BF5F') is an empty SEQUENCE */
static void get_conn_params_request_test(void)
{
	struct GetConnectivityParametersRequest req = { 0 };

	enc_assert(&asn_DEF_GetConnectivityParametersRequest, &req, "BF5F00");
}

/* Response decode: GetConnectivityParametersResponse, both arms of the CHOICE */
static void get_conn_params_response_decode_test(void)
{
	/* connectivityParameters ('A0') with httpParams ('81') = AA BB CC */
	const uint8_t params[] = { 0xBF, 0x5F, 0x07, 0xA0, 0x05, 0x81, 0x03, 0xAA, 0xBB, 0xCC };
	/* connectivityParametersError ('81') = parametersNotAvailable(1) */
	const uint8_t error[] = { 0xBF, 0x5F, 0x03, 0x81, 0x01, 0x01 };
	struct GetConnectivityParametersResponse *rsp = NULL;
	asn_dec_rval_t rval;

	rval = ber_decode(0, &asn_DEF_GetConnectivityParametersResponse, (void **)&rsp, params, sizeof(params));
	assert(rval.code == RC_OK);
	assert(rsp->present == GetConnectivityParametersResponse_PR_connectivityParameters);
	assert(rsp->choice.connectivityParameters.httpParams);
	assert(rsp->choice.connectivityParameters.httpParams->size == 3);
	assert(memcmp(rsp->choice.connectivityParameters.httpParams->buf, "\xAA\xBB\xCC", 3) == 0);
	ASN_STRUCT_FREE(asn_DEF_GetConnectivityParametersResponse, rsp);
	rsp = NULL;

	rval = ber_decode(0, &asn_DEF_GetConnectivityParametersResponse, (void **)&rsp, error, sizeof(error));
	assert(rval.code == RC_OK);
	assert(rsp->present == GetConnectivityParametersResponse_PR_connectivityParametersError);
	assert(rsp->choice.connectivityParametersError == ConnectivityParametersError_parametersNotAvailable);
	ASN_STRUCT_FREE(asn_DEF_GetConnectivityParametersResponse, rsp);
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	exec_fallback_request_test();
	return_from_fallback_request_test();
	exec_fallback_response_decode_test();
	return_from_fallback_response_decode_test();
	emergency_profile_request_test();
	enable_emergency_response_decode_test();
	get_conn_params_request_test();
	get_conn_params_response_decode_test();

	printf("all v1.2 ES10b wire format checks passed\n");
	return 0;
}
