// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_amp_valuegetters.cpp  -  KAT for CSTGAmp's Get* family -- all
 * 7 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_amp_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual/ctx-index facts the source
 * file's own decoder used -- not by re-using the .cpp file's C output
 * strings -- against the same deterministic non-trivial byte pattern as
 * the rest of the STG value-getter family's KATs: buf[i] = i times 0x9f
 * plus 0x37, all mod 0x100. ctx's own dynamic-index field at +0x4 is
 * fixed at 3, matching the established KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_amp.h"

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
	printf("CSTGAmp value-getter family known-answer test (7 methods)\n");
	printf("============================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGAmp *s = (CSTGAmp *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetLevel(ctx);
	check_eq("CSTGAmp::GetLevel value", CSTGParamsOwner::sValueGetterTemp.value, -1997976917L);
	check_eq("CSTGAmp::GetLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1997976917L);
	s->GetVelocityAmount(ctx);
	check_eq("CSTGAmp::GetVelocityAmount value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTGAmp::GetVelocityAmount displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);
	s->GetLFOAmount(ctx);
	check_eq("CSTGAmp::GetLFOAmount value", CSTGParamsOwner::sValueGetterTemp.value, -195709417L);
	check_eq("CSTGAmp::GetLFOAmount displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -195709417L);
	s->GetLevelAMSSource(ctx);
	check_eq("CSTGAmp::GetLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 23L);
	s->GetLevelAMSIntensity(ctx);
	check_eq("CSTGAmp::GetLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 2027502235L);
	check_eq("CSTGAmp::GetLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2027502235L);
	s->GetLFOAmountAMSSource(ctx);
	check_eq("CSTGAmp::GetLFOAmountAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -125L);
	s->GetLFOAmountAMSIntensity(ctx);
	check_eq("CSTGAmp::GetLFOAmountAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -465197561L);
	check_eq("CSTGAmp::GetLFOAmountAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -465197561L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
