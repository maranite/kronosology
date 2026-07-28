// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_multi_filter2pole_valuegetters.cpp  -  KAT for
 * CSTGMultiFilter2Pole's Get* family -- all 23 real weak-symbol
 * ctx-only candidates, see
 * ../src/engine/stg_multi_filter2pole_valuegetters.cpp.
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
#include "oa_stg_multi_filter2pole.h"

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
	printf("CSTGMultiFilter2Pole value-getter family known-answer test (23 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGMultiFilter2Pole *s = (CSTGMultiFilter2Pole *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetFilterType(ctx);
	check_eq("CSTGMultiFilter2Pole::GetFilterType value", CSTGParamsOwner::sValueGetterTemp.value, -85L);
	s->GetBypass(ctx);
	check_eq("CSTGMultiFilter2Pole::GetBypass value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetFrequency(ctx);
	check_eq("CSTGMultiFilter2Pole::GetFrequency value", CSTGParamsOwner::sValueGetterTemp.value, -970487575L);
	check_eq("CSTGMultiFilter2Pole::GetFrequency displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -970487575L);
	s->GetResonance(ctx);
	check_eq("CSTGMultiFilter2Pole::GetResonance value", CSTGParamsOwner::sValueGetterTemp.value, 1117979749L);
	check_eq("CSTGMultiFilter2Pole::GetResonance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1117979749L);
	s->GetTrim(ctx);
	check_eq("CSTGMultiFilter2Pole::GetTrim value", CSTGParamsOwner::sValueGetterTemp.value, -1105231647L);
	check_eq("CSTGMultiFilter2Pole::GetTrim displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1105231647L);
	s->GetEGIntensity(ctx);
	check_eq("CSTGMultiFilter2Pole::GetEGIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 983301213L);
	check_eq("CSTGMultiFilter2Pole::GetEGIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 983301213L);
	s->GetEGVelocity(ctx);
	check_eq("CSTGMultiFilter2Pole::GetEGVelocity value", CSTGParamsOwner::sValueGetterTemp.value, -1239975719L);
	check_eq("CSTGMultiFilter2Pole::GetEGVelocity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1239975719L);
	s->GetKeytrackIntensity(ctx);
	check_eq("CSTGMultiFilter2Pole::GetKeytrackIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 444324925L);
	check_eq("CSTGMultiFilter2Pole::GetKeytrackIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 444324925L);
	s->GetEGAMSSource(ctx);
	check_eq("CSTGMultiFilter2Pole::GetEGAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 53L);
	s->GetEGAMSIntensity(ctx);
	check_eq("CSTGMultiFilter2Pole::GetEGAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1762174791L);
	check_eq("CSTGMultiFilter2Pole::GetEGAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1762174791L);
	s->GetFreqAMS1Source(ctx);
	check_eq("CSTGMultiFilter2Pole::GetFreqAMS1Source value", CSTGParamsOwner::sValueGetterTemp.value, 80L);
	s->GetFreqAMS1Intensity(ctx);
	check_eq("CSTGMultiFilter2Pole::GetFreqAMS1Intensity value", CSTGParamsOwner::sValueGetterTemp.value, -1324190764L);
	check_eq("CSTGMultiFilter2Pole::GetFreqAMS1Intensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1324190764L);
	s->GetFreqAMS2Source(ctx);
	check_eq("CSTGMultiFilter2Pole::GetFreqAMS2Source value", CSTGParamsOwner::sValueGetterTemp.value, 107L);
	s->GetFreqAMS2Intensity(ctx);
	check_eq("CSTGMultiFilter2Pole::GetFreqAMS2Intensity value", CSTGParamsOwner::sValueGetterTemp.value, -869429521L);
	check_eq("CSTGMultiFilter2Pole::GetFreqAMS2Intensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -869429521L);
	s->GetResonanceAMSSource(ctx);
	check_eq("CSTGMultiFilter2Pole::GetResonanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -122L);
	s->GetResonanceAMSIntensity(ctx);
	check_eq("CSTGMultiFilter2Pole::GetResonanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -414668534L);
	check_eq("CSTGMultiFilter2Pole::GetResonanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -414668534L);
	s->GetOutputLevel(ctx);
	check_eq("CSTGMultiFilter2Pole::GetOutputLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1404310902L);
	check_eq("CSTGMultiFilter2Pole::GetOutputLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1404310902L);
	s->GetOutputLevelAMSSource(ctx);
	check_eq("CSTGMultiFilter2Pole::GetOutputLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 110L);
	s->GetOutputLevelAMSIntensity(ctx);
	check_eq("CSTGMultiFilter2Pole::GetOutputLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -818900494L);
	check_eq("CSTGMultiFilter2Pole::GetOutputLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -818900494L);
	s->GetLFOIntensity(ctx);
	check_eq("CSTGMultiFilter2Pole::GetLFOIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1509463863L);
	check_eq("CSTGMultiFilter2Pole::GetLFOIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1509463863L);
	s->GetLFOJSminusYIntensity(ctx);
	check_eq("CSTGMultiFilter2Pole::GetLFOJSminusYIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 444324925L);
	check_eq("CSTGMultiFilter2Pole::GetLFOJSminusYIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 444324925L);
	s->GetLFOAMSSource(ctx);
	check_eq("CSTGMultiFilter2Pole::GetLFOAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -14L);
	s->GetLFOAMSIntensity(ctx);
	check_eq("CSTGMultiFilter2Pole::GetLFOAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1404310902L);
	check_eq("CSTGMultiFilter2Pole::GetLFOAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1404310902L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
