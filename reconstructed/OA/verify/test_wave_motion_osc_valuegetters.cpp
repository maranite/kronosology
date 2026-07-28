// SPDX-License-Identifier: GPL-2.0
/*
 * test_wave_motion_osc_valuegetters.cpp  -  KAT for CWaveMotionOsc's
 * Get* family -- 23 of 23 real weak-symbol candidates, see
 * ../src/engine/wave_motion_osc_valuegetters.cpp.
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
#include "oa_wave_motion_osc.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld\n", label, got); return; }
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x800
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CWaveMotionOsc value-getter family known-answer test (23 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CWaveMotionOsc *s = (CWaveMotionOsc *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetOscLevel(ctx);
	check_eq("CWaveMotionOsc::GetOscLevel value", CSTGParamsOwner::sValueGetterTemp.value, 2027502235L);
	check_eq("CWaveMotionOsc::GetOscLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2027502235L);
	s->GetOscLevelAMSSource(ctx);
	check_eq("CWaveMotionOsc::GetOscLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -109L);
	s->GetOscLevelAMSIntensity(ctx);
	check_eq("CWaveMotionOsc::GetOscLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -195709417L);
	check_eq("CWaveMotionOsc::GetOscLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -195709417L);
	s->GetDecay(ctx);
	check_eq("CWaveMotionOsc::GetDecay value", CSTGParamsOwner::sValueGetterTemp.value, 259051826L);
	check_eq("CWaveMotionOsc::GetDecay displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 259051826L);
	s->GetDecayAMSSource(ctx);
	check_eq("CWaveMotionOsc::GetDecayAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 42L);
	s->GetDecayAMSIntensity(ctx);
	check_eq("CWaveMotionOsc::GetDecayAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1947447890L);
	check_eq("CWaveMotionOsc::GetDecayAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1947447890L);
	s->GetRelease(ctx);
	check_eq("CWaveMotionOsc::GetRelease value", CSTGParamsOwner::sValueGetterTemp.value, -1509463863L);
	check_eq("CWaveMotionOsc::GetRelease displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1509463863L);
	s->GetReleaseAMSSource(ctx);
	check_eq("CWaveMotionOsc::GetReleaseAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -63L);
	s->GetReleaseAMSIntensity(ctx);
	check_eq("CWaveMotionOsc::GetReleaseAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 579068997L);
	check_eq("CWaveMotionOsc::GetReleaseAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 579068997L);
	s->GetKeyDownNoiseLevel(ctx);
	check_eq("CWaveMotionOsc::GetKeyDownNoiseLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1033830240L);
	check_eq("CWaveMotionOsc::GetKeyDownNoiseLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1033830240L);
	s->GetKeyDownNoiseLevelAMSSource(ctx);
	check_eq("CWaveMotionOsc::GetKeyDownNoiseLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 88L);
	s->GetKeyDownNoiseLevelAMSIntensity(ctx);
	check_eq("CWaveMotionOsc::GetKeyDownNoiseLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1189446692L);
	check_eq("CWaveMotionOsc::GetKeyDownNoiseLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1189446692L);
	s->GetKeyUpNoiseLevel(ctx);
	check_eq("CWaveMotionOsc::GetKeyUpNoiseLevel value", CSTGParamsOwner::sValueGetterTemp.value, -734685449L);
	check_eq("CWaveMotionOsc::GetKeyUpNoiseLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -734685449L);
	s->GetKeyUpNoiseLevelAMSSource(ctx);
	check_eq("CWaveMotionOsc::GetKeyUpNoiseLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -17L);
	s->GetKeyUpNoiseLevelAMSIntensity(ctx);
	check_eq("CWaveMotionOsc::GetKeyUpNoiseLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1353781875L);
	check_eq("CWaveMotionOsc::GetKeyUpNoiseLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1353781875L);
	s->GetNoiseTone(ctx);
	check_eq("CWaveMotionOsc::GetNoiseTone value", CSTGParamsOwner::sValueGetterTemp.value, 1808543118L);
	check_eq("CWaveMotionOsc::GetNoiseTone displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1808543118L);
	s->GetNoiseToneAMSSource(ctx);
	check_eq("CWaveMotionOsc::GetNoiseToneAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -122L);
	s->GetNoiseToneAMSIntensity(ctx);
	check_eq("CWaveMotionOsc::GetNoiseToneAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -414668534L);
	check_eq("CWaveMotionOsc::GetNoiseToneAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -414668534L);
	s->GetHammerWidth(ctx);
	check_eq("CWaveMotionOsc::GetHammerWidth value", CSTGParamsOwner::sValueGetterTemp.value, 40092709L);
	check_eq("CWaveMotionOsc::GetHammerWidth displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 40092709L);
	s->GetHammerWidthAMSSource(ctx);
	check_eq("CWaveMotionOsc::GetHammerWidthAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 29L);
	s->GetHammerWidthAMSIntensity(ctx);
	check_eq("CWaveMotionOsc::GetHammerWidthAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 2128560289L);
	check_eq("CWaveMotionOsc::GetHammerWidthAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2128560289L);
	s->GetSlope(ctx);
	check_eq("CWaveMotionOsc::GetSlope value", CSTGParamsOwner::sValueGetterTemp.value, -2132720989L);
	check_eq("CWaveMotionOsc::GetSlope displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2132720989L);
	s->GetRibbon(ctx);
	check_eq("CWaveMotionOsc::GetRibbon value", CSTGParamsOwner::sValueGetterTemp.value, -60965345L);
	check_eq("CWaveMotionOsc::GetRibbon displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -60965345L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
