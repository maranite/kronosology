// SPDX-License-Identifier: GPL-2.0
#include <cstdio>
#include <cstring>
#include "oa_stg_vpm_osc.h"

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
	printf("CSTGVPMOsc value-getter family known-answer test (44 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGVPMOsc *s = (CSTGVPMOsc *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetOscOnOff(ctx);
	check_eq("CSTGVPMOsc::GetOscOnOff value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetOscMode(ctx);
	check_eq("CSTGVPMOsc::GetOscMode value", CSTGParamsOwner::sValueGetterTemp.value, 23L);
	s->GetFreqRatioCoarse(ctx);
	check_eq("CSTGVPMOsc::GetFreqRatioCoarse value", CSTGParamsOwner::sValueGetterTemp.value, 182L);
	s->GetFreqRatioFine(ctx);
	check_eq("CSTGVPMOsc::GetFreqRatioFine value", CSTGParamsOwner::sValueGetterTemp.value, -2987L);
	s->GetFreqOffsetCoarse(ctx);
	check_eq("CSTGVPMOsc::GetFreqOffsetCoarse value", CSTGParamsOwner::sValueGetterTemp.value, 12947L);
	s->GetFreqOffsetFine(ctx);
	check_eq("CSTGVPMOsc::GetFreqOffsetFine value", CSTGParamsOwner::sValueGetterTemp.value, -47L);
	s->GetInitialPhase(ctx);
	check_eq("CSTGVPMOsc::GetInitialPhase value", CSTGParamsOwner::sValueGetterTemp.value, 3952L);
	s->GetPhaseSync(ctx);
	check_eq("CSTGVPMOsc::GetPhaseSync value", CSTGParamsOwner::sValueGetterTemp.value, -82L);
	s->GetFMInputLevel1(ctx);
	check_eq("CSTGVPMOsc::GetFMInputLevel1 value", CSTGParamsOwner::sValueGetterTemp.value, 713813069L);
	check_eq("CSTGVPMOsc::GetFMInputLevel1 displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 713813069L);
	s->GetFMInputLevel2(ctx);
	check_eq("CSTGVPMOsc::GetFMInputLevel2 value", CSTGParamsOwner::sValueGetterTemp.value, -1509463863L);
	check_eq("CSTGVPMOsc::GetFMInputLevel2 displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1509463863L);
	s->GetFeedbackLevel(ctx);
	check_eq("CSTGVPMOsc::GetFeedbackLevel value", CSTGParamsOwner::sValueGetterTemp.value, 579068997L);
	check_eq("CSTGVPMOsc::GetFeedbackLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 579068997L);
	s->GetFeedbackLevelAMSSource(ctx);
	check_eq("CSTGVPMOsc::GetFeedbackLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 61L);
	s->GetFeedbackLevelAMSIntensity(ctx);
	check_eq("CSTGVPMOsc::GetFeedbackLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1627430719L);
	check_eq("CSTGVPMOsc::GetFeedbackLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1627430719L);
	s->GetFeedbackPrePost(ctx);
	check_eq("CSTGVPMOsc::GetFeedbackPrePost value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetUseCommonPitchMod(ctx);
	check_eq("CSTGVPMOsc::GetUseCommonPitchMod value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetPitchAMS1Source(ctx);
	check_eq("CSTGVPMOsc::GetPitchAMS1Source value", CSTGParamsOwner::sValueGetterTemp.value, -44L);
	s->GetPitchAMS1Intensity(ctx);
	check_eq("CSTGVPMOsc::GetPitchAMS1Intensity value", CSTGParamsOwner::sValueGetterTemp.value, -1189446692L);
	check_eq("CSTGVPMOsc::GetPitchAMS1Intensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1189446692L);
	s->GetPitchAMS1IntModSource(ctx);
	check_eq("CSTGVPMOsc::GetPitchAMS1IntModSource value", CSTGParamsOwner::sValueGetterTemp.value, 115L);
	s->GetPitchAMS1IntModIntensity(ctx);
	check_eq("CSTGVPMOsc::GetPitchAMS1IntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 899086168L);
	check_eq("CSTGVPMOsc::GetPitchAMS1IntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 899086168L);
	s->GetPitchAMS2Source(ctx);
	check_eq("CSTGVPMOsc::GetPitchAMS2Source value", CSTGParamsOwner::sValueGetterTemp.value, -114L);
	s->GetPitchAMS2Intensity(ctx);
	check_eq("CSTGVPMOsc::GetPitchAMS2Intensity value", CSTGParamsOwner::sValueGetterTemp.value, -279924462L);
	check_eq("CSTGVPMOsc::GetPitchAMS2Intensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -279924462L);
	s->GetLPFCutoff(ctx);
	check_eq("CSTGVPMOsc::GetLPFCutoff value", CSTGParamsOwner::sValueGetterTemp.value, 174836781L);
	check_eq("CSTGVPMOsc::GetLPFCutoff displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 174836781L);
	s->GetWaveshaperTable(ctx);
	check_eq("CSTGVPMOsc::GetWaveshaperTable value", CSTGParamsOwner::sValueGetterTemp.value, -87L);
	s->GetWaveshaperDrive(ctx);
	check_eq("CSTGVPMOsc::GetWaveshaperDrive value", CSTGParamsOwner::sValueGetterTemp.value, 629598024L);
	check_eq("CSTGVPMOsc::GetWaveshaperDrive displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 629598024L);
	s->GetWaveshaperDriveKeySlope(ctx);
	check_eq("CSTGVPMOsc::GetWaveshaperDriveKeySlope value", CSTGParamsOwner::sValueGetterTemp.value, -1593678908L);
	check_eq("CSTGVPMOsc::GetWaveshaperDriveKeySlope displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1593678908L);
	s->GetWaveshaperDriveKeySlopeHighOnly(ctx);
	check_eq("CSTGVPMOsc::GetWaveshaperDriveKeySlopeHighOnly value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetWaveshaperDriveAMSSource(ctx);
	check_eq("CSTGVPMOsc::GetWaveshaperDriveAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -68L);
	s->GetWaveshaperDriveAMSIntensity(ctx);
	check_eq("CSTGVPMOsc::GetWaveshaperDriveAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 494853952L);
	check_eq("CSTGVPMOsc::GetWaveshaperDriveAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 494853952L);
	s->GetWaveshaperOffset(ctx);
	check_eq("CSTGVPMOsc::GetWaveshaperOffset value", CSTGParamsOwner::sValueGetterTemp.value, 949615195L);
	check_eq("CSTGVPMOsc::GetWaveshaperOffset displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 949615195L);
	s->GetWaveshaperOffsetAMSSource(ctx);
	check_eq("CSTGVPMOsc::GetWaveshaperOffsetAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 83L);
	s->GetWaveshaperOffsetAMSIntensity(ctx);
	check_eq("CSTGVPMOsc::GetWaveshaperOffsetAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1273661737L);
	check_eq("CSTGVPMOsc::GetWaveshaperOffsetAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1273661737L);
	s->GetWaveshaperHPFCutoff(ctx);
	check_eq("CSTGVPMOsc::GetWaveshaperHPFCutoff value", CSTGParamsOwner::sValueGetterTemp.value, -818900494L);
	check_eq("CSTGVPMOsc::GetWaveshaperHPFCutoff displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -818900494L);
	s->GetWaveshaperOutputLevel(ctx);
	check_eq("CSTGVPMOsc::GetWaveshaperOutputLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1269566830L);
	check_eq("CSTGVPMOsc::GetWaveshaperOutputLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1269566830L);
	s->GetRingModCrossfade(ctx);
	check_eq("CSTGVPMOsc::GetRingModCrossfade value", CSTGParamsOwner::sValueGetterTemp.value, -953644566L);
	check_eq("CSTGVPMOsc::GetRingModCrossfade displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -953644566L);
	s->GetRingModCrossfadeAMSSource(ctx);
	check_eq("CSTGVPMOsc::GetRingModCrossfadeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -30L);
	s->GetRingModCrossfadeAMSIntensity(ctx);
	check_eq("CSTGVPMOsc::GetRingModCrossfadeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1134822758L);
	check_eq("CSTGVPMOsc::GetRingModCrossfadeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1134822758L);
	s->GetVolume(ctx);
	check_eq("CSTGVPMOsc::GetVolume value", CSTGParamsOwner::sValueGetterTemp.value, 1589584001L);
	check_eq("CSTGVPMOsc::GetVolume displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1589584001L);
	s->GetVolumeEGSelect(ctx);
	check_eq("CSTGVPMOsc::GetVolumeEGSelect value", CSTGParamsOwner::sValueGetterTemp.value, -3L);
	s->GetVolumeVelocitySensitivity(ctx);
	check_eq("CSTGVPMOsc::GetVolumeVelocitySensitivity value", CSTGParamsOwner::sValueGetterTemp.value, 2044345244L);
	check_eq("CSTGVPMOsc::GetVolumeVelocitySensitivity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2044345244L);
	s->GetVolumeAMSSource(ctx);
	check_eq("CSTGVPMOsc::GetVolumeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 16L);
	s->GetVolumeAMSIntensity(ctx);
	check_eq("CSTGVPMOsc::GetVolumeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -178866408L);
	check_eq("CSTGVPMOsc::GetVolumeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -178866408L);
	s->GetVolumeAMSIntModSource(ctx);
	check_eq("CSTGVPMOsc::GetVolumeAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -81L);
	s->GetVolumeAMSIntModIntensity(ctx);
	check_eq("CSTGVPMOsc::GetVolumeAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1909601172L);
	check_eq("CSTGVPMOsc::GetVolumeAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1909601172L);
	s->GetVolumeAMSMode(ctx);
	check_eq("CSTGVPMOsc::GetVolumeAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, 78L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
