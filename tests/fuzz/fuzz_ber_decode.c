/*
 * Copyright (c) 2026 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 */

/* Fuzz target for the BER decoder over the externally-reachable roots: the
 * eIM (ESipa response), the eUICC (info/notification/installation PDUs) and
 * the profile package are all remote input. First input byte selects the
 * root, the rest is fed to ber_decode.
 *
 * Built with -fsanitize=fuzzer (ENABLE_FUZZ=ON, clang) this is a libFuzzer
 * binary; without it a driver main() replays the given files once, which
 * doubles as the ctest smoke run over the corpus. */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <BoundProfilePackage.h>
#include <EUICCInfo2.h>
#include <EsipaMessageFromEimToIpa.h>
#include <ProfileInstallationResult.h>
#include <RetrieveNotificationsListResponse.h>
#include <SGP32-EUICCInfo2.h>

static const asn_TYPE_descriptor_t *const roots[] = {
	&asn_DEF_EsipaMessageFromEimToIpa,
	&asn_DEF_BoundProfilePackage,
	&asn_DEF_EUICCInfo2,
	&asn_DEF_SGP32_EUICCInfo2,
	&asn_DEF_RetrieveNotificationsListResponse,
	&asn_DEF_ProfileInstallationResult,
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	const asn_TYPE_descriptor_t *td;
	void *st = NULL;

	if (size < 1)
		return 0;
	td = roots[data[0] % (sizeof(roots) / sizeof(roots[0]))];
	(void)ber_decode(0, td, &st, data + 1, size - 1);
	ASN_STRUCT_FREE(*td, st);
	return 0;
}

#ifndef IPA_FUZZ_ENGINE
/* Replay driver: run each file through the target once. */
int main(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		FILE *f = fopen(argv[i], "rb");
		static uint8_t buf[65536];
		size_t n;

		if (!f) {
			fprintf(stderr, "cannot open %s\n", argv[i]);
			return 1;
		}
		n = fread(buf, 1, sizeof(buf), f);
		fclose(f);
		LLVMFuzzerTestOneInput(buf, n);
	}
	printf("replayed %d input(s)\n", argc - 1);
	return 0;
}
#endif
