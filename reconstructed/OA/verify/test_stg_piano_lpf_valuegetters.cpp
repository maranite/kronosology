// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_piano_lpf_valuegetters.cpp  -  KAT for CSTGPianoLPF's Get*
 * family -- all 9 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_piano_lpf_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own
 * decoder used -- not by re-using the .cpp file's C output strings --
 * against the same deterministic non-trivial byte pattern as the rest
 * of the STG value-getter family's KATs: buf[i] = i times 0x9f plus
 * 0x37, all mod 0x100.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_piano_lpf.h"

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
	printf("CSTGPianoLPF value-getter family known-answer test (9 methods)\n");
	printf("================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGPianoLPF *s = (CSTGPianoLPF *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetFrequency(ctx);
	check_eq("CSTGPianoLPF::GetFrequency value", CSTGParamsOwner::sValueGetterTemp.value, 1437996920L);
	check_eq("CSTGPianoLPF::GetFrequency displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1437996920L);
	s->GetFreqKeyTrackIntensity(ctx);
	check_eq("CSTGPianoLPF::GetFreqKeyTrackIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -785214476L);
	check_eq("CSTGPianoLPF::GetFreqKeyTrackIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -785214476L);
	s->GetFreqAMS1Source(ctx);
	check_eq("CSTGPianoLPF::GetFreqAMS1Source value", CSTGParamsOwner::sValueGetterTemp.value, -20L);
	s->GetFreqAMS1Intensity(ctx);
	check_eq("CSTGPianoLPF::GetFreqAMS1Intensity value", CSTGParamsOwner::sValueGetterTemp.value, 1303252848L);
	check_eq("CSTGPianoLPF::GetFreqAMS1Intensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1303252848L);
	s->GetFreqAMS2Source(ctx);
	check_eq("CSTGPianoLPF::GetFreqAMS2Source value", CSTGParamsOwner::sValueGetterTemp.value, 7L);
	s->GetFreqAMS2Intensity(ctx);
	check_eq("CSTGPianoLPF::GetFreqAMS2Intensity value", CSTGParamsOwner::sValueGetterTemp.value, 1758014091L);
	check_eq("CSTGPianoLPF::GetFreqAMS2Intensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1758014091L);
	s->GetFreqAMS1IntensityAMSSource(ctx);
	check_eq("CSTGPianoLPF::GetFreqAMS1IntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 34L);
	s->GetFreqAMS1IntensityAMSIntensity(ctx);
	check_eq("CSTGPianoLPF::GetFreqAMS1IntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -2082191962L);
	check_eq("CSTGPianoLPF::GetFreqAMS1IntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2082191962L);
	s->GetLidPosition(ctx);
	check_eq("CSTGPianoLPF::GetLidPosition value", CSTGParamsOwner::sValueGetterTemp.value, -1627430719L);
	check_eq("CSTGPianoLPF::GetLidPosition displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1627430719L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
