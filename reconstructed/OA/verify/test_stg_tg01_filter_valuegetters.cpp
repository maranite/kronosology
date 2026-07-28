// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_tg01_filter_valuegetters.cpp  -  KAT for CSTGTG01Filter's
 * single Get* method, see
 * ../src/engine/stg_tg01_filter_valuegetters.cpp.
 *
 * Expected value computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed facts the source file's own decoder
 * used -- not by re-using the .cpp file's C output string -- against the
 * same deterministic non-trivial byte pattern as the rest of the STG
 * value-getter family's KATs: buf[i] = i times 0x9f plus 0x37, all mod
 * 0x100.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_tg01_filter.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x900
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGTG01Filter value-getter family known-answer test (1 method)\n");
	printf("=================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);

	CSTGTG01Filter *s = (CSTGTG01Filter *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetRouting(ctx);
	check_eq("CSTGTG01Filter::GetRouting value", CSTGParamsOwner::sValueGetterTemp.value, 36L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
