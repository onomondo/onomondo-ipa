/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Checks the SGP.32 v1.2 ESipa wire formats that changed relative to v1.0.1:
 *  - ProvideEimPackageResult is a SEQUENCE (eidValue + eimPackageResult CHOICE)
 *  - ProvideEimPackageResultResponse is a CHOICE (acks / empty / error)
 *  - HandleNotificationEsipa wraps pendingNotification in an explicit [0] tag
 *  - IpaEuiccData carries defaultSmdpAddress at tag '81' and eimTransactionId at '87'
 *  - ProfileDownloadTriggerResult carries a mandatory profileDownloadErrorReason
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <EsipaMessageFromIpaToEim.h>
#include <EsipaMessageFromEimToIpa.h>
#include <IpaEuiccDataResponse.h>
#include <ProfileDownloadTriggerResult.h>

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

static const uint8_t eid[16] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};
static const uint8_t txid[3] = { 0xAA, 0xBB, 0xCC };

/* ProvideEimPackageResult: SEQUENCE with eidValue ('5A') and the error arm ('A0') of the
 * eimPackageResult CHOICE, carrying eimTransactionId ('80') and eimPackageResultErrorCode ('02') */
static void provide_eim_package_result_error_test(void)
{
	struct EsipaMessageFromIpaToEim msg = { 0 };
	OCTET_STRING_t eid_value = { 0 };
	TransactionId_t transaction_id = { 0 };

	eid_value.buf = (uint8_t *)eid;
	eid_value.size = sizeof(eid);
	transaction_id.buf = (uint8_t *)txid;
	transaction_id.size = sizeof(txid);

	msg.present = EsipaMessageFromIpaToEim_PR_provideEimPackageResult;
	msg.choice.provideEimPackageResult.eidValue = &eid_value;
	msg.choice.provideEimPackageResult.eimPackageResult.present =
	    EimPackageResult_PR_eimPackageResultResponseError;
	msg.choice.provideEimPackageResult.eimPackageResult.choice.eimPackageResultResponseError.eimTransactionId =
	    &transaction_id;
	msg.choice.provideEimPackageResult.eimPackageResult.choice.eimPackageResultResponseError.
	    eimPackageResultErrorCode = EimPackageResultErrorCode_unknownPackage;

	enc_assert(&asn_DEF_EsipaMessageFromIpaToEim, &msg,
		   "BF501C5A10000102030405060708090A0B0C0D0E0FA0088003AABBCC020102");
}

/* ProfileDownloadTriggerResult: eimTransactionId ('82') and the profileDownloadError arm with
 * the (v1.2) mandatory profileDownloadErrorReason ('80') */
static void profile_download_trigger_result_error_test(void)
{
	struct ProfileDownloadTriggerResult trigger_result = { 0 };
	TransactionId_t transaction_id = { 0 };

	transaction_id.buf = (uint8_t *)txid;
	transaction_id.size = sizeof(txid);

	trigger_result.eimTransactionId = &transaction_id;
	trigger_result.profileDownloadTriggerResultData.present =
	    ProfileDownloadTriggerResult__profileDownloadTriggerResultData_PR_profileDownloadError;
	trigger_result.profileDownloadTriggerResultData.choice.profileDownloadError.profileDownloadErrorReason =
	    ProfileDownloadTriggerResult__profileDownloadTriggerResultData__profileDownloadError__profileDownloadErrorReason_undefinedError;

	enc_assert(&asn_DEF_ProfileDownloadTriggerResult, &trigger_result, "BF540A8203AABBCC300380017F");
}

/* IpaEuiccData: defaultSmdpAddress moved to tag '81', eimTransactionId echo at tag '87' */
static void ipa_euicc_data_test(void)
{
	struct IpaEuiccDataResponse rsp = { 0 };
	UTF8String_t smdp_addr = { 0 };
	TransactionId_t transaction_id = { 0 };

	smdp_addr.buf = (uint8_t *)"x";
	smdp_addr.size = 1;
	transaction_id.buf = (uint8_t *)txid;
	transaction_id.size = sizeof(txid);

	rsp.present = IpaEuiccDataResponse_PR_ipaEuiccData;
	rsp.choice.ipaEuiccData.defaultSmdpAddress = &smdp_addr;
	rsp.choice.ipaEuiccData.eimTransactionId = &transaction_id;

	enc_assert(&asn_DEF_IpaEuiccDataResponse, &rsp, "BF520AA0088101788703AABBCC");
}

/* HandleNotificationEsipa: the pendingNotification arm is wrapped in an explicit [0] ('A0')
 * in v1.2 (in v1.0.1 the notification tags appeared directly inside 'BF3D') */
static void handle_notification_test(void)
{
	struct EsipaMessageFromIpaToEim msg = { 0 };
	struct CompactOtherSignedNotification *notif;
	uint8_t pmo_bits = 0x40; /* notificationInstall */
	uint8_t sig = 0xEE;

	msg.present = EsipaMessageFromIpaToEim_PR_handleNotificationEsipa;
	msg.choice.handleNotificationEsipa.present = HandleNotificationEsipa_PR_pendingNotification;
	msg.choice.handleNotificationEsipa.choice.pendingNotification.present =
	    SGP32_PendingNotification_PR_compactOtherSignedNotification;
	notif = &msg.choice.handleNotificationEsipa.choice.pendingNotification.choice.compactOtherSignedNotification;

	notif->tbsOtherNotification.seqNumber = 7;
	notif->tbsOtherNotification.profileManagementOperation.buf = &pmo_bits;
	notif->tbsOtherNotification.profileManagementOperation.size = 1;
	notif->tbsOtherNotification.profileManagementOperation.bits_unused = 6;
	notif->tbsOtherNotification.notificationAddress.buf = (uint8_t *)"a";
	notif->tbsOtherNotification.notificationAddress.size = 1;
	notif->euiccNotificationSignature.buf = &sig;
	notif->euiccNotificationSignature.size = 1;

	enc_assert(&asn_DEF_EsipaMessageFromIpaToEim, &msg,
		   "BF3D15A013A111BF2F0A800107810206400C01615F3701EE");
}

/* GetEimPackageRequest: the v1.2 stateChangeCause ('81') must travel together with
 * notifyStateChange ('80'); a plain poll carries neither */
static void get_eim_package_request_test(void)
{
	struct EsipaMessageFromIpaToEim msg = { 0 };
	NULL_t notify_state_change = 0;
	StateChangeCause_t cause = StateChangeCause_fallback;

	msg.present = EsipaMessageFromIpaToEim_PR_getEimPackageRequest;
	msg.choice.getEimPackageRequest.eidValue.buf = (uint8_t *)eid;
	msg.choice.getEimPackageRequest.eidValue.size = sizeof(eid);

	enc_assert(&asn_DEF_EsipaMessageFromIpaToEim, &msg, "BF4F125A10000102030405060708090A0B0C0D0E0F");

	msg.choice.getEimPackageRequest.notifyStateChange = &notify_state_change;
	msg.choice.getEimPackageRequest.stateChangeCause = &cause;

	enc_assert(&asn_DEF_EsipaMessageFromIpaToEim, &msg,
		   "BF4F175A10000102030405060708090A0B0C0D0E0F8000810101");
}

/* ProvideEimPackageResultResponse: v1.2 CHOICE decode of all three alternatives */
static void provide_eim_package_result_response_decode_test(void)
{
	/* eimAcknowledgements ('BF53') with one SequenceNumber (5) */
	const uint8_t acks[] = { 0xBF, 0x50, 0x06, 0xBF, 0x53, 0x03, 0x80, 0x01, 0x05 };
	/* emptyResponse (empty SEQUENCE) */
	const uint8_t empty[] = { 0xBF, 0x50, 0x02, 0x30, 0x00 };
	/* provideEimPackageResultError undefinedError(127) */
	const uint8_t error[] = { 0xBF, 0x50, 0x03, 0x02, 0x01, 0x7F };
	struct EsipaMessageFromEimToIpa *msg = NULL;
	asn_dec_rval_t rval;

	rval = ber_decode(0, &asn_DEF_EsipaMessageFromEimToIpa, (void **)&msg, acks, sizeof(acks));
	assert(rval.code == RC_OK);
	assert(msg->present == EsipaMessageFromEimToIpa_PR_provideEimPackageResultResponse);
	assert(msg->choice.provideEimPackageResultResponse.present ==
	       ProvideEimPackageResultResponse_PR_eimAcknowledgements);
	assert(msg->choice.provideEimPackageResultResponse.choice.eimAcknowledgements.list.count == 1);
	assert(*msg->choice.provideEimPackageResultResponse.choice.eimAcknowledgements.list.array[0] == 5);
	ASN_STRUCT_FREE(asn_DEF_EsipaMessageFromEimToIpa, msg);
	msg = NULL;

	rval = ber_decode(0, &asn_DEF_EsipaMessageFromEimToIpa, (void **)&msg, empty, sizeof(empty));
	assert(rval.code == RC_OK);
	assert(msg->present == EsipaMessageFromEimToIpa_PR_provideEimPackageResultResponse);
	assert(msg->choice.provideEimPackageResultResponse.present == ProvideEimPackageResultResponse_PR_emptyResponse);
	ASN_STRUCT_FREE(asn_DEF_EsipaMessageFromEimToIpa, msg);
	msg = NULL;

	rval = ber_decode(0, &asn_DEF_EsipaMessageFromEimToIpa, (void **)&msg, error, sizeof(error));
	assert(rval.code == RC_OK);
	assert(msg->present == EsipaMessageFromEimToIpa_PR_provideEimPackageResultResponse);
	assert(msg->choice.provideEimPackageResultResponse.present ==
	       ProvideEimPackageResultResponse_PR_provideEimPackageResultError);
	assert(msg->choice.provideEimPackageResultResponse.choice.provideEimPackageResultError == 127);
	ASN_STRUCT_FREE(asn_DEF_EsipaMessageFromEimToIpa, msg);
}

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	provide_eim_package_result_error_test();
	profile_download_trigger_result_error_test();
	ipa_euicc_data_test();
	handle_notification_test();
	get_eim_package_request_test();
	provide_eim_package_result_response_decode_test();

	printf("all v1.2 ESipa wire format checks passed\n");
	return 0;
}
