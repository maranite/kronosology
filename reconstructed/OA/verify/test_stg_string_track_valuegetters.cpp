// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_string_track_valuegetters.cpp  -  KAT for CSTGStringTrack's
 * single Get* method, see
 * ../src/engine/stg_string_track_valuegetters.cpp.
 *
 * Expected value computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/ctx-index facts the source file's own decoder
 * used -- not by re-using the .cpp file's C output string -- against the
 * same deterministic non-trivial byte pattern as the rest of the STG
 * value-getter family's KATs: buf[i] = i times 0x9f plus 0x37, all mod
 * 0x100. ctx's own dynamic-index field at +0x4 is fixed at 3, matching
 * the established KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_string_track.h"

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
	printf("CSTGStringTrack value-getter family known-answer test (1 method)\n");
	printf("==================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGStringTrack *s = (CSTGStringTrack *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetStringValue(ctx);
	check_eq("CSTGStringTrack::GetStringValue value", CSTGParamsOwner::sValueGetterTemp.value, -60965345L);
	check_eq("CSTGStringTrack::GetStringValue displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -60965345L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
