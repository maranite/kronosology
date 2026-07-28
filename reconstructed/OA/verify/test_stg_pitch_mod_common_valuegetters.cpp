// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_pitch_mod_common_valuegetters.cpp  -  KAT for
 * CSTGPitchModCommon's Get* family -- all 5 real weak-symbol ctx-only
 * candidates, see ../src/engine/stg_pitch_mod_common_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own
 * decoder used -- not by re-using the .cpp file's C output strings --
 * against the same deterministic non-trivial byte pattern as the rest of
 * the STG value-getter family's KATs: buf[i] = i times 0x9f plus 0x37,
 * all mod 0x100. ctx's own dynamic-index field at +0x4 is fixed at 3,
 * matching the established KAT convention, though this class has no
 * ctx-indexed methods to exercise it.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_pitch_mod_common.h"

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
	printf("CSTGPitchModCommon value-getter family known-answer test (5 methods)\n");
	printf("======================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGPitchModCommon *s = (CSTGPitchModCommon *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetLFOSelect(ctx);
	check_eq("CSTGPitchModCommon::GetLFOSelect value", CSTGParamsOwner::sValueGetterTemp.value, 228L);
	s->GetLFOAmount(ctx);
	check_eq("CSTGPitchModCommon::GetLFOAmount value", CSTGParamsOwner::sValueGetterTemp.value, -1374719791L);
	check_eq("CSTGPitchModCommon::GetLFOAmount displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1374719791L);
	s->GetJSYToLFOAmount(ctx);
	check_eq("CSTGPitchModCommon::GetJSYToLFOAmount value", CSTGParamsOwner::sValueGetterTemp.value, 713813069L);
	check_eq("CSTGPitchModCommon::GetJSYToLFOAmount displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 713813069L);
	s->GetLFOAMSSource(ctx);
	check_eq("CSTGPitchModCommon::GetLFOAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 69L);
	s->GetLFOAMSIntensity(ctx);
	check_eq("CSTGPitchModCommon::GetLFOAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1509463863L);
	check_eq("CSTGPitchModCommon::GetLFOAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1509463863L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
