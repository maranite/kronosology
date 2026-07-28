// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_simple2pole_valuegetters.cpp  -  KAT for CSTGSimple2Pole's
 * Get* family -- all 13 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_simple2pole_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own
 * decoder used -- not by re-using the .cpp file's C output strings --
 * against the same deterministic non-trivial byte pattern as the rest
 * of the STG value-getter family's KATs: buf[i] = i times 0x9f plus
 * 0x37, all mod 0x100. ctx's own dynamic-index field at +0x4 is fixed
 * at 3, matching the established KAT convention, though this class has
 * no ctx-indexed methods to exercise it.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_simple2pole.h"

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
	printf("CSTGSimple2Pole value-getter family known-answer test (13 methods)\n");
	printf("=====================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGSimple2Pole *s = (CSTGSimple2Pole *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetBypass(ctx);
	check_eq("CSTGSimple2Pole::GetBypass value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetFilterType(ctx);
	check_eq("CSTGSimple2Pole::GetFilterType value", CSTGParamsOwner::sValueGetterTemp.value, -85L);
	s->GetTrim(ctx);
	check_eq("CSTGSimple2Pole::GetTrim value", CSTGParamsOwner::sValueGetterTemp.value, -1105231647L);
	check_eq("CSTGSimple2Pole::GetTrim displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1105231647L);
	s->GetFrequency(ctx);
	check_eq("CSTGSimple2Pole::GetFrequency value", CSTGParamsOwner::sValueGetterTemp.value, -970487575L);
	check_eq("CSTGSimple2Pole::GetFrequency displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -970487575L);
	s->GetResonance(ctx);
	check_eq("CSTGSimple2Pole::GetResonance value", CSTGParamsOwner::sValueGetterTemp.value, 1117979749L);
	check_eq("CSTGSimple2Pole::GetResonance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1117979749L);
	s->GetResonanceAMSSource(ctx);
	check_eq("CSTGSimple2Pole::GetResonanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 42L);
	s->GetResonanceAMSIntensity(ctx);
	check_eq("CSTGSimple2Pole::GetResonanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1947447890L);
	check_eq("CSTGSimple2Pole::GetResonanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1947447890L);
	s->GetFreqAMS1Source(ctx);
	check_eq("CSTGSimple2Pole::GetFreqAMS1Source value", CSTGParamsOwner::sValueGetterTemp.value, -39L);
	s->GetFreqAMS1Intensity(ctx);
	check_eq("CSTGSimple2Pole::GetFreqAMS1Intensity value", CSTGParamsOwner::sValueGetterTemp.value, 983301213L);
	check_eq("CSTGSimple2Pole::GetFreqAMS1Intensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 983301213L);
	s->GetFreqAMS2Source(ctx);
	check_eq("CSTGSimple2Pole::GetFreqAMS2Source value", CSTGParamsOwner::sValueGetterTemp.value, -12L);
	s->GetFreqAMS2Intensity(ctx);
	check_eq("CSTGSimple2Pole::GetFreqAMS2Intensity value", CSTGParamsOwner::sValueGetterTemp.value, 1437996920L);
	check_eq("CSTGSimple2Pole::GetFreqAMS2Intensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1437996920L);
	s->GetFreqAMS1IntensityAMSSource(ctx);
	check_eq("CSTGSimple2Pole::GetFreqAMS1IntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 15L);
	s->GetFreqAMS1IntensityAMSIntensity(ctx);
	check_eq("CSTGSimple2Pole::GetFreqAMS1IntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1892758163L);
	check_eq("CSTGSimple2Pole::GetFreqAMS1IntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1892758163L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
