/*
 * Copyright (c) 2025 Onomondo ApS & sysmocom - s.f.m.c. GmbH. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <unistd.h>
#include <onomondo/ipa/utils.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/ipad.h>

#define DEFAULT_READER_NUMBER 0
#define DEFAULT_CHANNEL_NUMBER 1
#define DEFAULT_TAC "12345678"
#define DEFAULT_NVSTATE_PATH "./nvstate.bin"
#define DEFAULT_ESIPA_REQ_RETRIES 3

bool running = true;

bool prfle_inst_consent(char *sm_dp_plus_address, char *ac_token)
{
	char user_input;
	printf("PLEASE CONSENT TO PROFILE INSTALLATION:\n");
	printf("smdp+: %s\n", sm_dp_plus_address);
	printf("ac-token: %s\n", ac_token);
	printf("Consent (Y/N)? ");
	user_input = getchar();
	if (user_input == 'Y' || user_input == 'y')
		return true;
	return false;
}

static void print_help(void)
{
	printf("options:\n");
	printf(" -h .................. print this text.\n");
	printf(" -t TAC .............. set TAC (default: %s)\n", DEFAULT_TAC);
	printf(" -e eimId ............ set preferred eIM (in case the eUICC has multiple)\n");
	printf(" -r N ................ set reader number (default: %d)\n", DEFAULT_READER_NUMBER);
	printf(" -c N ................ set logical channel number (default: %d)\n", DEFAULT_CHANNEL_NUMBER);
	printf(" -f PATH ............. set initial eIM configuration\n");
	printf(" -m .................. reset eUICC memory\n");
	printf(" -n PATH ............. path to nvstate file (default: %s)\n", DEFAULT_NVSTATE_PATH);
	printf(" -y NUM .............. number of retries for ESipa requests (default: %u)\n",
	       DEFAULT_ESIPA_REQ_RETRIES);
	printf(" -a .................. ask end user for consent\n");
	printf(" -C .................. CA (Certificate Authority) Bundle file\n");
	printf(" -S .................. disable HTTPS\n");
	printf(" -I .................. disable SSL certificate verification (insecure)\n");
	printf(" -E .................. emulate IoT eUICC (compatibility mode to use consumer eUICCs)\n");
	printf(" -1 .................. force the IPAd to process only one eUICC package (debug, use with caution)\n");
}

struct ipa_buf *load_ber_from_file(char *dir, char *file)
{
	char path[PATH_MAX] = { 0 };
	FILE *ber_file = NULL;
	struct ipa_buf *ber = NULL;
	size_t ber_size;

	if (dir)
		strcpy(path, dir);
	strcat(path, file);

	ber_file = fopen(path, "r");
	assert(ber_file);

	fseek(ber_file, 0L, SEEK_END);
	ber_size = ftell(ber_file);
	rewind(ber_file);

	ber = ipa_buf_alloc(ber_size + 1);
	assert(ber);

	ber->len = fread(ber->data, sizeof(char), ber->data_len, ber_file);
	fclose(ber_file);
	IPA_LOGP(SMAIN, LINFO, "loaded BER data from file %s, size: %zu\n", path, ber->len);
	return ber;
}

struct ipa_buf *load_nvstate_from_file(char *path)
{
	FILE *file_ptr = NULL;
	struct ipa_buf *nvstate = NULL;
	size_t file_size;

	file_ptr = fopen(path, "r");
	if (!file_ptr) {
		IPA_LOGP(SMAIN, LERROR, "unable to load nvstate from file %s -- a new nvstate will be created.\n",
			 path);
		return NULL;
	}

	fseek(file_ptr, 0L, SEEK_END);
	file_size = ftell(file_ptr);
	rewind(file_ptr);

	nvstate = ipa_buf_alloc(file_size);
	assert(nvstate);

	nvstate->len = fread(nvstate->data, sizeof(char), nvstate->data_len, file_ptr);
	fclose(file_ptr);
	IPA_LOGP(SMAIN, LINFO, "loaded nvstate from file %s, size: %zu\n", path, nvstate->data_len);

	return nvstate;
}

void save_nvstate_to_file(char *path, struct ipa_buf *nvstate)
{
	FILE *file_ptr = NULL;

	file_ptr = fopen(path, "w");
	if (!file_ptr) {
		IPA_LOGP(SMAIN, LERROR, "unable to save nvstate from file %s!\n", path);
		return;
	}

	fwrite(nvstate->data, sizeof(char), nvstate->data_len, file_ptr);
	fclose(file_ptr);
	IPA_LOGP(SMAIN, LINFO, "saved nvstate to file %s, size: %zu\n", path, nvstate->data_len);
}

static void sig_usr1(int signum)
{
	running = false;
}

int main(int argc, char **argv)
{
	struct ipa_config cfg = { 0 };
	struct ipa_context *ctx = NULL;
	int opt;
	int rc;
	char *getopt_initial_eim_cfg_file = NULL;
	bool getopt_euicc_memory_reset = false;
	char *getopt_nvstate_path = DEFAULT_NVSTATE_PATH;
	struct ipa_buf *nvstate_load = NULL;
	struct ipa_buf *nvstate_save = NULL;
	bool getopt_one_euicc_pkg_only = false;

	signal(SIGUSR1, sig_usr1);

	printf("IPAd!\n");

	/* Populate configuration with default values */
	cfg.reader_num = DEFAULT_READER_NUMBER;
	cfg.euicc_channel = DEFAULT_CHANNEL_NUMBER;
	ipa_binary_from_hexstr(cfg.tac, sizeof(cfg.tac), DEFAULT_TAC);
	cfg.esipa_req_retries = DEFAULT_ESIPA_REQ_RETRIES;

	/* Overwrite configuration values with user defined parameters */
	while (1) {
		opt = getopt(argc, argv, "ht:e:r:c:f:mn:C:SIEy:a1");
		if (opt == -1)
			break;

		switch (opt) {
		case 'h':
			print_help();
			exit(0);
			break;
		case 't':
			ipa_binary_from_hexstr(cfg.tac, sizeof(cfg.tac), optarg);
			break;
		case 'e':
			cfg.preferred_eim_id = optarg;
			break;
		case 'r':
			cfg.reader_num = atoi(optarg);
			break;
		case 'c':
			cfg.euicc_channel = atoi(optarg);
			break;
		case 'f':
			getopt_initial_eim_cfg_file = optarg;
			break;
		case 'm':
			getopt_euicc_memory_reset = true;
			break;
		case 'n':
			getopt_nvstate_path = optarg;
			break;
		case 'C':
			cfg.eim_cabundle = optarg;
			break;
		case 'S':
			cfg.eim_disable_ssl = true;
			break;
		case 'I':
			cfg.eim_disable_ssl_verif = true;
			break;
		case 'E':
			cfg.iot_euicc_emu_enabled = true;
			break;
		case 'y':
			cfg.esipa_req_retries = atoi(optarg);
			break;
		case 'a':
			cfg.prfle_inst_consent_cb = prfle_inst_consent;
			break;
		case '1':
			getopt_one_euicc_pkg_only = true;
			break;
		default:
			printf("unhandled option: %c!\n", opt);
			break;
		};
	}

	/* Display current config */
	printf("parameter:\n");
	printf(" nvstate path: %s\n", getopt_nvstate_path);
	printf(" preferred_eim_id = %s\n", cfg.preferred_eim_id ? cfg.preferred_eim_id : "(first configured eIM)");
	printf(" reader_num = %d\n", cfg.reader_num);
	printf(" euicc_channel = %d\n", cfg.euicc_channel);
	if (cfg.eim_cabundle)
		printf(" eim_cabundle = %s\n", cfg.eim_cabundle);
	printf(" eim_disable_ssl = %d\n", cfg.eim_disable_ssl);
	printf(" eim_disable_ssl_verif = %d\n", cfg.eim_disable_ssl_verif);
	printf(" tac = %s\n", ipa_hexdump(cfg.tac, sizeof(cfg.tac)));
	printf(" iot_euicc_emu_enabled = %u\n", cfg.iot_euicc_emu_enabled);
	printf(" esipa_req_retries = %u\n", cfg.esipa_req_retries);
	printf(" refresh_flag = %u\n", cfg.refresh_flag);
	printf("\n");

	if (cfg.eim_cabundle) {
		rc = access(cfg.eim_cabundle, R_OK);
		if (rc < 0) {
			IPA_LOGP(SMAIN, LERROR, "error accessing CA bundle %s: %s\n", cfg.eim_cabundle,
				 strerror(errno));
			goto leave;
		}
	}

	/* Create a new IPA context */
	nvstate_load = load_nvstate_from_file(getopt_nvstate_path);
	ctx = ipa_new_ctx(&cfg, nvstate_load);
	if (!ctx) {
		IPA_LOGP(SMAIN, LERROR, "cannot create context!\n");
		rc = -EINVAL;
		goto leave;
	}

	/* Initialize IPA */
	IPA_LOGP(SMAIN, LINFO, "-----------------------------8<-----------------------------\n");
	rc = ipa_init(ctx);
	if (rc < 0) {
		IPA_LOGP(SMAIN, LERROR, "IPAd initialization failed!\n");
		rc = -EINVAL;
		goto leave;
	}

	if (getopt_initial_eim_cfg_file) {
		/* Load initial eIM configuration */
		struct ipa_buf *eim_cfg = load_ber_from_file(NULL, getopt_initial_eim_cfg_file);
		ipa_add_init_eim_cfg(ctx, eim_cfg);
		IPA_FREE(eim_cfg);
	} else if (getopt_euicc_memory_reset) {
		/* Perform an eUICC memory reset */
		ipa_euicc_mem_rst(ctx, true, true, true, true, true);
	} else {
		IPA_LOGP(SMAIN, LINFO, "-----------------------------8<-----------------------------\n");
		rc = eim_init(ctx);
		if (rc < 0) {
			IPA_LOGP(SMAIN, LERROR, "eIM initialization failed!\n");
			rc = -EINVAL;
			goto leave;
		}

		while (running) {
			IPA_LOGP(SMAIN, LINFO, "-----------------------------8<-----------------------------\n");
			rc = ipa_poll(ctx);

			switch (rc) {
			case IPA_POLL_AGAIN_WHEN_ONLINE:
				/* ipa_poll asks us to wait with the next poll cycle until we have a stable IP
				 * connection. In this example we assume that IP connectivity is always available. */
				IPA_LOGP(SMAIN, LINFO, "poll cycle continues normally (profile change)\n");
				rc = 0;
				break;
			case IPA_POLL_AGAIN:
				/* ipa_poll asks us to continue polling normally */
				rc = 0;
				if (getopt_one_euicc_pkg_only) {
					IPA_LOGP(SMAIN, LINFO, "forcefully stopping poll cycle upon user decision!\n");
					goto leave;
				} else {
					IPA_LOGP(SMAIN, LINFO, "poll cycle continues normally\n");
					break;
				}
			case IPA_POLL_AGAIN_LATER:
				/* ipa_poll tells us that we may poll less frequently, so just exit. */
				IPA_LOGP(SMAIN, LERROR, "poll cycle ends normally\n");
				rc = 0;
				goto leave;
			default:
				/* We got a negative return code from ipa_poll. This means something does not work
				 * normally. In a productive setup we would continue calling ipa_poll a few more times
				 * to see if the cause is a temporary problem. After that we would free the context
				 * using ipa_free_ctx and start over. */
				IPA_LOGP(SMAIN, LERROR, "poll cycle ends due to error (%d)\n", rc);
				rc = -EINVAL;
				goto leave;
			}
		}
	}

leave:
	IPA_LOGP(SMAIN, LINFO, "-----------------------------8<-----------------------------\n");
	nvstate_save = ipa_free_ctx(ctx);
	if (nvstate_save)
		save_nvstate_to_file(getopt_nvstate_path, nvstate_save);
	IPA_FREE(nvstate_load);
	IPA_FREE(nvstate_save);
	return rc;
}
