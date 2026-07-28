// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_polysix_valuegetters.cpp  -  KAT for CSTGPolysix's Get*() family
 * (71 methods, see ../src/engine/stg_polysix_valuegetters.cpp).
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed (offset, ctx-index*stride, width, signed, dual) facts the
 * source file's own decoder used -- not by re-using the .cpp file's C
 * output strings -- against the same deterministic non-trivial byte
 * pattern as the CSTGString/CSTGOrganModelPatch/CSTGMS20 KATs (buf[i] =
 * (i*0x9f+0x37) & 0xff). ctx's own dynamic-index field (+0x4) is fixed at
 * 3 for every ctx-indexed getter, matching the established KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_polysix.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld\n", label, got); return; }
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x300
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGPolysix value-getter family known-answer test (71 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGPolysix *s = (CSTGPolysix *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetAmpAttenuator(ctx);
	check_eq("CSTGPolysix::GetAmpAttenuator value", CSTGParamsOwner::sValueGetterTemp.value, -1930604881L);
	check_eq("CSTGPolysix::GetAmpAttenuator displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1930604881L);
	s->GetAmpAttenuatorAMSIntModIntensity(ctx);
	check_eq("CSTGPolysix::GetAmpAttenuatorAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -2065348953L);
	check_eq("CSTGPolysix::GetAmpAttenuatorAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2065348953L);
	s->GetAmpAttenuatorAMSIntModSource(ctx);
	check_eq("CSTGPolysix::GetAmpAttenuatorAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -62L);
	s->GetAmpAttenuatorAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetAmpAttenuatorAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 141150763L);
	check_eq("CSTGPolysix::GetAmpAttenuatorAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 141150763L);
	s->GetAmpAttenuatorAMSSource(ctx);
	check_eq("CSTGPolysix::GetAmpAttenuatorAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 35L);
	s->GetAmpEGMode(ctx);
	check_eq("CSTGPolysix::GetAmpEGMode value", CSTGParamsOwner::sValueGetterTemp.value, 210L);
	s->GetAmpEGModeAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetAmpEGModeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 113L);
	s->GetAmpEGModeAMSSource(ctx);
	check_eq("CSTGPolysix::GetAmpEGModeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 16L);
	s->GetAnalog(ctx);
	check_eq("CSTGPolysix::GetAnalog value", CSTGParamsOwner::sValueGetterTemp.value, -1526306872L);
	check_eq("CSTGPolysix::GetAnalog displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1526306872L);
	s->GetExtModAmpGainIntensity(ctx);
	check_eq("CSTGPolysix::GetExtModAmpGainIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 157993772L);
	check_eq("CSTGPolysix::GetExtModAmpGainIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 157993772L);
	s->GetExtModFilterCutoffIntensity(ctx);
	check_eq("CSTGPolysix::GetExtModFilterCutoffIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1913761872L);
	check_eq("CSTGPolysix::GetExtModFilterCutoffIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1913761872L);
	s->GetExtModMGLevelIntensity(ctx);
	check_eq("CSTGPolysix::GetExtModMGLevelIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -2048505944L);
	check_eq("CSTGPolysix::GetExtModMGLevelIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2048505944L);
	s->GetExtModOscPulseWidthIntensity(ctx);
	check_eq("CSTGPolysix::GetExtModOscPulseWidthIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 292737844L);
	check_eq("CSTGPolysix::GetExtModOscPulseWidthIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 292737844L);
	s->GetExtModSource(ctx);
	check_eq("CSTGPolysix::GetExtModSource value", CSTGParamsOwner::sValueGetterTemp.value, 23249700L);
	s->GetFilterCutoff(ctx);
	check_eq("CSTGPolysix::GetFilterCutoff value", CSTGParamsOwner::sValueGetterTemp.value, -414668534L);
	check_eq("CSTGPolysix::GetFilterCutoff displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -414668534L);
	s->GetFilterCutoffAMSIntModIntensity(ctx);
	check_eq("CSTGPolysix::GetFilterCutoffAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -549412606L);
	check_eq("CSTGPolysix::GetFilterCutoffAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -549412606L);
	s->GetFilterCutoffAMSIntModSource(ctx);
	check_eq("CSTGPolysix::GetFilterCutoffAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, 29L);
	s->GetFilterCutoffAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetFilterCutoffAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1673799046L);
	check_eq("CSTGPolysix::GetFilterCutoffAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1673799046L);
	s->GetFilterCutoffAMSSource(ctx);
	check_eq("CSTGPolysix::GetFilterCutoffAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 126L);
	s->GetFilterEGIntensity(ctx);
	check_eq("CSTGPolysix::GetFilterEGIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1269566830L);
	check_eq("CSTGPolysix::GetFilterEGIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1269566830L);
	s->GetFilterEGIntensityAMSIntModIntensity(ctx);
	check_eq("CSTGPolysix::GetFilterEGIntensityAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1134822758L);
	check_eq("CSTGPolysix::GetFilterEGIntensityAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1134822758L);
	s->GetFilterEGIntensityAMSIntModSource(ctx);
	check_eq("CSTGPolysix::GetFilterEGIntensityAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -127L);
	s->GetFilterEGIntensityAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetFilterEGIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -953644566L);
	check_eq("CSTGPolysix::GetFilterEGIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -953644566L);
	s->GetFilterEGIntensityAMSSource(ctx);
	check_eq("CSTGPolysix::GetFilterEGIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -30L);
	s->GetFilterKeyboardTrack(ctx);
	check_eq("CSTGPolysix::GetFilterKeyboardTrack value", CSTGParamsOwner::sValueGetterTemp.value, -44122336L);
	check_eq("CSTGPolysix::GetFilterKeyboardTrack displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -44122336L);
	s->GetFilterKeyboardTrackAMSIntModIntensity(ctx);
	check_eq("CSTGPolysix::GetFilterKeyboardTrackAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -178866408L);
	check_eq("CSTGPolysix::GetFilterKeyboardTrackAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -178866408L);
	s->GetFilterKeyboardTrackAMSIntModSource(ctx);
	check_eq("CSTGPolysix::GetFilterKeyboardTrackAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, 51L);
	s->GetFilterKeyboardTrackAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetFilterKeyboardTrackAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 2044345244L);
	check_eq("CSTGPolysix::GetFilterKeyboardTrackAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2044345244L);
	s->GetFilterKeyboardTrackAMSSource(ctx);
	check_eq("CSTGPolysix::GetFilterKeyboardTrackAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -108L);
	s->GetFilterResonance(ctx);
	check_eq("CSTGPolysix::GetFilterResonance value", CSTGParamsOwner::sValueGetterTemp.value, -1711645764L);
	check_eq("CSTGPolysix::GetFilterResonance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1711645764L);
	s->GetFilterResonanceAMSIntModIntensity(ctx);
	check_eq("CSTGPolysix::GetFilterResonanceAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1846389836L);
	check_eq("CSTGPolysix::GetFilterResonanceAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1846389836L);
	s->GetFilterResonanceAMSIntModSource(ctx);
	check_eq("CSTGPolysix::GetFilterResonanceAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -49L);
	s->GetFilterResonanceAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetFilterResonanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 360109880L);
	check_eq("CSTGPolysix::GetFilterResonanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 360109880L);
	s->GetFilterResonanceAMSSource(ctx);
	check_eq("CSTGPolysix::GetFilterResonanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 48L);
	s->GetMGDestination(ctx);
	check_eq("CSTGPolysix::GetMGDestination value", CSTGParamsOwner::sValueGetterTemp.value, 19L);
	s->GetMGDestinationAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetMGDestinationAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -78L);
	s->GetMGDestinationAMSSource(ctx);
	check_eq("CSTGPolysix::GetMGDestinationAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 81L);
	s->GetMGLevel(ctx);
	check_eq("CSTGPolysix::GetMGLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1050607713L);
	check_eq("CSTGPolysix::GetMGLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1050607713L);
	s->GetMGLevelAMSIntModIntensity(ctx);
	check_eq("CSTGPolysix::GetMGLevelAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 915929177L);
	check_eq("CSTGPolysix::GetMGLevelAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 915929177L);
	s->GetMGLevelAMSIntModSource(ctx);
	check_eq("CSTGPolysix::GetMGLevelAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, 116L);
	s->GetMGLevelAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetMGLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1172603683L);
	check_eq("CSTGPolysix::GetMGLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1172603683L);
	s->GetMGLevelAMSSource(ctx);
	check_eq("CSTGPolysix::GetMGLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -43L);
	s->GetOscOctave(ctx);
	check_eq("CSTGPolysix::GetOscOctave value", CSTGParamsOwner::sValueGetterTemp.value, 171L);
	s->GetOscOctaveAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetOscOctaveAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 74L);
	s->GetOscOctaveAMSSource(ctx);
	check_eq("CSTGPolysix::GetOscOctaveAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -23L);
	s->GetOscPulseWidth(ctx);
	check_eq("CSTGPolysix::GetOscPulseWidth value", CSTGParamsOwner::sValueGetterTemp.value, 2078031262L);
	check_eq("CSTGPolysix::GetOscPulseWidth displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2078031262L);
	s->GetOscPulseWidthAMSIntModIntensity(ctx);
	check_eq("CSTGPolysix::GetOscPulseWidthAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1943287190L);
	check_eq("CSTGPolysix::GetOscPulseWidthAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1943287190L);
	s->GetOscPulseWidthAMSIntModSource(ctx);
	check_eq("CSTGPolysix::GetOscPulseWidthAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -79L);
	s->GetOscPulseWidthAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetOscPulseWidthAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -145180390L);
	check_eq("CSTGPolysix::GetOscPulseWidthAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -145180390L);
	s->GetOscPulseWidthAMSSource(ctx);
	check_eq("CSTGPolysix::GetOscPulseWidthAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 18L);
	s->GetOscTranspose(ctx);
	check_eq("CSTGPolysix::GetOscTranspose value", CSTGParamsOwner::sValueGetterTemp.value, 1707485064L);
	check_eq("CSTGPolysix::GetOscTranspose displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1707485064L);
	s->GetOscTransposeAMSIntModIntensity(ctx);
	check_eq("CSTGPolysix::GetOscTransposeAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1572740992L);
	check_eq("CSTGPolysix::GetOscTransposeAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1572740992L);
	s->GetOscTransposeAMSIntModSource(ctx);
	check_eq("CSTGPolysix::GetOscTransposeAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -101L);
	s->GetOscTransposeAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetOscTransposeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -515726588L);
	check_eq("CSTGPolysix::GetOscTransposeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -515726588L);
	s->GetOscTransposeAMSSource(ctx);
	check_eq("CSTGPolysix::GetOscTransposeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -4L);
	s->GetOscTune(ctx);
	check_eq("CSTGPolysix::GetOscTune value", CSTGParamsOwner::sValueGetterTemp.value, 393795898L);
	check_eq("CSTGPolysix::GetOscTune displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 393795898L);
	s->GetOscTuneAMSIntModIntensity(ctx);
	check_eq("CSTGPolysix::GetOscTuneAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 259051826L);
	check_eq("CSTGPolysix::GetOscTuneAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 259051826L);
	s->GetOscTuneAMSIntModSource(ctx);
	check_eq("CSTGPolysix::GetOscTuneAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, 77L);
	s->GetOscTuneAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetOscTuneAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1812703818L);
	check_eq("CSTGPolysix::GetOscTuneAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1812703818L);
	s->GetOscTuneAMSSource(ctx);
	check_eq("CSTGPolysix::GetOscTuneAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -82L);
	s->GetOscVibratoIntensity(ctx);
	check_eq("CSTGPolysix::GetOscVibratoIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -919958548L);
	check_eq("CSTGPolysix::GetOscVibratoIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -919958548L);
	s->GetOscVibratoIntensityAMSIntModIntensity(ctx);
	check_eq("CSTGPolysix::GetOscVibratoIntensityAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1054702620L);
	check_eq("CSTGPolysix::GetOscVibratoIntensityAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1054702620L);
	s->GetOscVibratoIntensityAMSIntModSource(ctx);
	check_eq("CSTGPolysix::GetOscVibratoIntensityAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -1L);
	s->GetOscVibratoIntensityAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetOscVibratoIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1168508776L);
	check_eq("CSTGPolysix::GetOscVibratoIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1168508776L);
	s->GetOscVibratoIntensityAMSSource(ctx);
	check_eq("CSTGPolysix::GetOscVibratoIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 96L);
	s->GetOscWaveform(ctx);
	check_eq("CSTGPolysix::GetOscWaveform value", CSTGParamsOwner::sValueGetterTemp.value, 80L);
	s->GetOscWaveformAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetOscWaveformAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -17L);
	s->GetOscWaveformAMSSource(ctx);
	check_eq("CSTGPolysix::GetOscWaveformAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -114L);
	s->GetSubOsc(ctx);
	check_eq("CSTGPolysix::GetSubOsc value", CSTGParamsOwner::sValueGetterTemp.value, 45L);
	s->GetSubOscAMSIntensity(ctx);
	check_eq("CSTGPolysix::GetSubOscAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -52L);
	s->GetSubOscAMSSource(ctx);
	check_eq("CSTGPolysix::GetSubOscAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 107L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
