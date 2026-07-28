// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_keytrack_valuegetters.cpp  -  KAT for CSTGKeyTrack's Get*
 * family -- all 7 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_keytrack_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed facts the source file's own decoder
 * used -- not by re-using the .cpp file's C output strings -- against
 * the same deterministic non-trivial byte pattern as the rest of the
 * STG value-getter family's KATs: buf[i] = i times 0x9f plus 0x37, all
 * mod 0x100. This class has no ctx-dynamic-index methods, so ctx's own
 * fields are never read, but the buffer is still filled the same way
 * for consistency with the family's KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_keytrack.h"

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
	printf("CSTGKeyTrack value-getter family known-answer test (7 methods)\n");
	printf("============================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGKeyTrack *s = (CSTGKeyTrack *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetLowKey(ctx);
	check_eq("CSTGKeyTrack::GetLowKey value", CSTGParamsOwner::sValueGetterTemp.value, 171L);
	s->GetMidKey(ctx);
	check_eq("CSTGKeyTrack::GetMidKey value", CSTGParamsOwner::sValueGetterTemp.value, 74L);
	s->GetHighKey(ctx);
	check_eq("CSTGKeyTrack::GetHighKey value", CSTGParamsOwner::sValueGetterTemp.value, 233L);
	s->GetLowRamp(ctx);
	check_eq("CSTGKeyTrack::GetLowRamp value", CSTGParamsOwner::sValueGetterTemp.value, -120L);
	s->GetMidLowRamp(ctx);
	check_eq("CSTGKeyTrack::GetMidLowRamp value", CSTGParamsOwner::sValueGetterTemp.value, 39L);
	s->GetMidHighRamp(ctx);
	check_eq("CSTGKeyTrack::GetMidHighRamp value", CSTGParamsOwner::sValueGetterTemp.value, -58L);
	s->GetHighRamp(ctx);
	check_eq("CSTGKeyTrack::GetHighRamp value", CSTGParamsOwner::sValueGetterTemp.value, 101L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
