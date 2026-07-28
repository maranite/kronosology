// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_moss_amp_valuegetters.cpp  -  KAT for CSTGMOSSAmp's Get*
 * family -- all 6 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_moss_amp_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual/stride facts the source file's own
 * decoder used -- not by re-using the .cpp file's C output strings --
 * against the same deterministic non-trivial byte pattern as the rest of
 * the STG value-getter family's KATs: buf[i] = i times 0x9f plus 0x37, all
 * mod 0x100, with ctx's own dynamic-index field at +0x4 fixed at 3, same
 * as every other ctx-indexed class's own KAT in this family.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_moss_amp.h"

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
	printf("CSTGMOSSAmp value-getter family known-answer test (6 methods)\n");
	printf("============================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGMOSSAmp *s = (CSTGMOSSAmp *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetLevel(ctx);
	check_eq("CSTGMOSSAmp::GetLevel value", CSTGParamsOwner::sValueGetterTemp.value, -1997976917L);
	check_eq("CSTGMOSSAmp::GetLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1997976917L);
	s->GetAMSSource(ctx);
	check_eq("CSTGMOSSAmp::GetAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 112L);
	s->GetAMSIntensity(ctx);
	check_eq("CSTGMOSSAmp::GetAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -785214476L);
	check_eq("CSTGMOSSAmp::GetAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -785214476L);
	s->GetAMSIntensityAMSSource(ctx);
	check_eq("CSTGMOSSAmp::GetAMSIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -90L);
	s->GetAMSIntensityAMSIntensity(ctx);
	check_eq("CSTGMOSSAmp::GetAMSIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 124307754L);
	check_eq("CSTGMOSSAmp::GetAMSIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 124307754L);
	s->GetVelocityAmount(ctx);
	check_eq("CSTGMOSSAmp::GetVelocityAmount value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTGMOSSAmp::GetVelocityAmount displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
