/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 */

#include <stdio.h>
#include "src/ipa/libipa/activation_code.h"

void parse_ac(char *ac)
{
	struct ipa_activation_code *ac_decoded;
	printf("Encoded activation code: \"%s\"\n", ac);
	ac_decoded = ipa_activation_code_parse(ac);
	ipa_activation_code_dump(ac_decoded, 0, SMAIN, LINFO);
	ipa_activation_code_free(ac_decoded);
	printf("\n");
}

int main(int argc, char **argv)
{
	parse_ac("1$SMDP.EXAMPLE.COM$04386-AGYFT-A74Y8-3F815");
	parse_ac("1$SMDP.EXAMPLE.COM$04386-AGYFT-A74Y8-3F815$$1");
	parse_ac("1$SMDP.EXAMPLE.COM$04386-AGYFT-A74Y8-3F815$1.3.6.1.4.1.31746$1");
	parse_ac("1$SMDP.EXAMPLE.COM$04386-AGYFT-A74Y8-3F815$1.3.6.1.4.1.31746");
	parse_ac("1$SMDP.EXAMPLE.COM$$1.3.6.1.4.1.31746");
	parse_ac("1$SMDP.EXAMPLE.NET$KL14XA-8C7RLY$1.3.6.1.4.1.3174");
	parse_ac("1$SMDP.EXAMPLE.COM$$$$$$$$$$$$$$$$$$$$$$$$$$");

	/* When the activation code is provided via a QR code it has to be prefixed
	 * with "LPA:", see also GSMA SGP.22, section 4.1 */
	parse_ac("LPA:1$SMDP.EXAMPLE.COM$04386-AGYFT-A74Y8-3F815");

	/* Invalid */
	parse_ac("xyz14390lojij");
	parse_ac("");
	parse_ac("1$");
	parse_ac("LPA:1$");
	parse_ac("LPA:1$$$$$$$$$$$$$$$$$$$");
	parse_ac("\x10$\xAA$\xBB$\xCC");
	parse_ac("1$SMDP.EXAMPLE.COM");
	parse_ac(NULL);

	return 0;
}

/* Stubs */
void *ipa_http_init(const char *cabundle, bool no_verif)
{
	return NULL;
}

struct ipa_buf *ipa_http_req(void *http_ctx, const struct ipa_buf *req, const char *url)
{
	return NULL;
}

void ipa_http_close(void *http_ctx)
{
	return;
}

void ipa_http_free(void *http_ctx)
{
	return;
}

void *ipa_scard_init(unsigned int reader_num)
{
	return NULL;
}

int ipa_scard_reset(void *scard_ctx)
{
	return 0;
}

int ipa_scard_atr(void *scard_ctx, struct ipa_buf *atr)
{
	return 0;
}

int ipa_scard_transceive(void *scard_ctx, struct ipa_buf *res, const struct ipa_buf *req)
{
	return 0;
}

int ipa_scard_free(void *scard_ctx)
{
	return 0;
}
