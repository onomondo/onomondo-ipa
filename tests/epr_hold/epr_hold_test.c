/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * An eUICC Package Result waiting for its onset (after a profile switch) survives an eUICC that does not
 * answer Step #9, ES10b.RetrieveNotificationsList (SGP.32 section 3.3.1): ipa_poll keeps it and retries
 * the onset three times, without polling the eIM, before it drops the result from RAM.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <onomondo/ipa/ipad.h>
#include <onomondo/ipa/http.h>
#include <onomondo/ipa/scard.h>
#include <onomondo/ipa/mem.h>
#include <onomondo/ipa/utils.h>
#include "context.h"
#include "es10b_load_euicc_pkg.h"
#include "proc_euicc_pkg_dwnld_exec.h"

/* The eUICC is gone: every APDU fails on the transport. */
static int apdus;

int ipa_scard_transceive(void *scard_ctx, struct ipa_buf *res, const struct ipa_buf *req)
{
	(void)scard_ctx;
	(void)res;
	/* STORE DATA carrying RetrieveNotificationsList ('BF2B'), i.e. onset Step #9 */
	assert(req->len > 6 && req->data[1] == 0xE2 && req->data[5] == 0xBF && req->data[6] == 0x2B);
	apdus++;
	return -1;
}

/* GetEimPackage would go out here. */
static int http_reqs;

struct ipa_buf *ipa_http_req(void *http_ctx, const struct ipa_buf *req, const char *url)
{
	(void)http_ctx;
	(void)req;
	(void)url;
	http_reqs++;
	return NULL;
}

void ipa_http_close(void *http_ctx)
{
	(void)http_ctx;
}

/* Linked in with ipad.c, not called. */
void *ipa_scard_init(unsigned int reader_num)
{
	(void)reader_num;
	return NULL;
}

int ipa_scard_free(void *scard_ctx)
{
	(void)scard_ctx;
	return 0;
}

void *ipa_http_init(const char *cabundle, bool no_verif)
{
	(void)cabundle;
	(void)no_verif;
	return NULL;
}

void ipa_http_free(void *http_ctx)
{
	(void)http_ctx;
}

int main(void)
{
	struct ipa_config cfg = { 0 };
	struct ipa_context ctx = { 0 };
	struct ipa_proc_eucc_pkg_dwnld_exec_res *res = IPA_ALLOC_ZERO(struct ipa_proc_eucc_pkg_dwnld_exec_res);
	int i;

	/* the state ipa_proc_eucc_pkg_dwnld_exec leaves after an enable PSMO */
	res->call_onset = true;
	res->load_euicc_pkg_res = IPA_ALLOC_ZERO(struct ipa_es10b_load_euicc_pkg_res);
	res->load_euicc_pkg_res->profile_changed = true;
	res->load_euicc_pkg_res->res = IPA_ALLOC_ZERO(struct EuiccPackageResult);
	res->load_euicc_pkg_res->res->present = EuiccPackageResult_PR_euiccPackageResultSigned;
	res->load_euicc_pkg_res->res->choice.euiccPackageResultSigned.euiccPackageResultDataSigned.seqNumber = 7;
	ctx.cfg = &cfg;
	ctx.proc_eucc_pkg_dwnld_exec_res = res;

	/* the result is held and Step #9 retried at each poll; the eIM is not polled */
	for (i = 1; i <= 3; i++) {
		assert(ipa_poll(&ctx) == IPA_POLL_CHECK_SCARD);
		assert(ctx.proc_eucc_pkg_dwnld_exec_res == res);
		assert(res->call_onset);
		assert(apdus == i && http_reqs == 0);
	}

	/* the retries are used up: the result is dropped from RAM (the eUICC still holds it) */
	assert(ipa_poll(&ctx) == IPA_POLL_CHECK_SCARD);
	assert(ctx.proc_eucc_pkg_dwnld_exec_res == NULL);
	assert(apdus == 4 && http_reqs == 0);

	printf("epr_hold: ok\n");
	return 0;
}
