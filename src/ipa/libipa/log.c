/*
 * Copyrighct (c) 2025 Onomondo ApS. All rights reserved.
 *
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * Author: Philipp Maier <pmaier@sysmocom.de> / sysmocom - s.f.m.c. GmbH
 */

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>
#include <onomondo/ipa/log.h>
#include <onomondo/ipa/utils.h>

uint32_t ipa_log_mask = 0xffffffff;

/* TODO: how to modify log levels at runtime or from getopt */
static uint32_t subsys_lvl[_NUM_LOG_SUBSYS] = {
	[SMAIN] = LDEBUG,
	[SHTTP] = LDEBUG,
	[SSCARD] = LDEBUG,
	[SIPA] = LDEBUG,
	[SES10X] = LDEBUG,
	[SES10B] = LDEBUG,
	[SEUICC] = LDEBUG,
	[SESIPA] = LDEBUG,
};

static const char *subsys_str[_NUM_LOG_SUBSYS] = {
	[SMAIN] = "MAIN",
	[SHTTP] = "HTTP",
	[SSCARD] = "SCARD",
	[SIPA] = "IPA",
	[SES10X] = "ES10x",
	[SES10B] = "ES10b",
	[SEUICC] = "eUICC",
	[SESIPA] = "ESIPA",
};

static const char *level_str[_NUM_LOG_LEVEL] = {
	[LERROR] = "ERROR",
	[LINFO] = "INFO",
	[LDEBUG] = "DEBUG",
};

/*! print a log line (called by IPA_LOGP, do not call directly).
 *  \param[in] subsys log subsystem identifier.
 *  \param[in] level log level identifier.
 *  \param[in] file source file name.
 *  \param[in] line source file line.
 *  \param[in] format format string (followed by arguments). */
void ipa_logp(uint32_t subsys, uint32_t level, const char *file, int line, const char *format, ...)
{
	va_list ap;

	if (!(ipa_log_mask & (1 << subsys)))
		return;

	assert(subsys < IPA_ARRAY_SIZE(subsys_lvl));

	if (level > subsys_lvl[subsys])
		return;

	/* TODO: print file and line, but make it an optional feature that
	 * can be selected via commandline option. The reason for this is that
	 * the unit-tests may compare the log output against .err files and
	 * even on minor changes we would constantly upset the unit-tests. */

	fprintf(stderr, "%8s %8s ", subsys_str[subsys], level_str[level]);
	va_start(ap, format);
	vfprintf(stderr, format, ap);
	va_end(ap);
}
