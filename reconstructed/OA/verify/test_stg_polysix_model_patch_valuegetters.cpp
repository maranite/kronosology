// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_polysix_model_patch_valuegetters.cpp  -  KAT for
 * CSTGPolysixModelPatch's Get* family -- 48 of 48 real weak-symbol
 * candidates, see ../src/engine/stg_polysix_model_patch_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual/bitfield facts the source file's
 * own decoder used -- not by re-using the .cpp file's C output strings
 * -- against the same deterministic non-trivial byte pattern as the
 * rest of the STG value-getter family's KATs: buf[i] = i times 0x9f
 * plus 0x37, all mod 0x100.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_polysix_model_patch.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-55s %ld\n", label, got); return; }
	printf("  FAIL  %-55s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x900
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGPolysixModelPatch value-getter family known-answer test (48 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGPolysixModelPatch *s = (CSTGPolysixModelPatch *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetPWMSpeed(ctx);
	check_eq("CSTGPolysixModelPatch::GetPWMSpeed value", CSTGParamsOwner::sValueGetterTemp.value, 1219037803L);
	check_eq("CSTGPolysixModelPatch::GetPWMSpeed displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1219037803L);
	s->GetPWMSpeedAMSSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetPWMSpeedAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -33L);
	s->GetPWMSpeedAMSIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetPWMSpeedAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1004173593L);
	check_eq("CSTGPolysixModelPatch::GetPWMSpeedAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1004173593L);
	s->GetPWMSpeedAMSIntModSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetPWMSpeedAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, 126L);
	s->GetPWMSpeedAMSIntModIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetPWMSpeedAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1084293731L);
	check_eq("CSTGPolysixModelPatch::GetPWMSpeedAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1084293731L);
	s->GetEffectMode(ctx);
	check_eq("CSTGPolysixModelPatch::GetEffectMode value", CSTGParamsOwner::sValueGetterTemp.value, 29L);
	s->GetEffectModeAMSSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetEffectModeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 91L);
	s->GetEffectModeAMSIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetEffectModeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -68L);
	s->GetEffectSpread(ctx);
	check_eq("CSTGPolysixModelPatch::GetEffectSpread value", CSTGParamsOwner::sValueGetterTemp.value, -1273661737L);
	check_eq("CSTGPolysixModelPatch::GetEffectSpread displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1273661737L);
	s->GetEffectSpreadAMSSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetEffectSpreadAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 75L);
	s->GetEffectSpreadAMSIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetEffectSpreadAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 814871123L);
	check_eq("CSTGPolysixModelPatch::GetEffectSpreadAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 814871123L);
	s->GetEffectSpreadAMSIntModSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetEffectSpreadAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -22L);
	s->GetEffectSpreadAMSIntModIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetEffectSpreadAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1408405809L);
	check_eq("CSTGPolysixModelPatch::GetEffectSpreadAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1408405809L);
	s->GetEffectSpeedIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetEffectSpeedIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1543149881L);
	check_eq("CSTGPolysixModelPatch::GetEffectSpeedIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1543149881L);
	s->GetEffectSpeedIntensityAMSSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetEffectSpeedIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 59L);
	s->GetEffectSpeedIntensityAMSIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetEffectSpeedIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 545382979L);
	check_eq("CSTGPolysixModelPatch::GetEffectSpeedIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 545382979L);
	s->GetEffectSpeedIntensityAMSIntModSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetEffectSpeedIntensityAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -38L);
	s->GetEffectSpeedIntensityAMSIntModIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetEffectSpeedIntensityAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1661116737L);
	check_eq("CSTGPolysixModelPatch::GetEffectSpeedIntensityAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1661116737L);
	s->GetVolume(ctx);
	check_eq("CSTGPolysixModelPatch::GetVolume value", CSTGParamsOwner::sValueGetterTemp.value, -1795860809L);
	check_eq("CSTGPolysixModelPatch::GetVolume displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1795860809L);
	s->GetVolumeAMSSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetVolumeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 43L);
	s->GetVolumeAMSIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetVolumeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 275894835L);
	check_eq("CSTGPolysixModelPatch::GetVolumeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 275894835L);
	s->GetVolumeAMSIntModSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetVolumeAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -54L);
	s->GetVolumeAMSIntModIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetVolumeAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1930604881L);
	check_eq("CSTGPolysixModelPatch::GetVolumeAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1930604881L);
	s->GetArpeggiatorEnable(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorEnable value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetArpeggiatorEnableAMSSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorEnableAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 105L);
	s->GetArpeggiatorEnableAMSSwitchMode(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorEnableAMSSwitchMode value", CSTGParamsOwner::sValueGetterTemp.value, 8L);
	s->GetArpeggiatorKeySync(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorKeySync value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetArpeggiatorSpeed(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorSpeed value", CSTGParamsOwner::sValueGetterTemp.value, -2065348953L);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorSpeed displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2065348953L);
	s->GetArpeggiatorSpeedAMSSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorSpeedAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 27L);
	s->GetArpeggiatorSpeedAMSIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorSpeedAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 6406691L);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorSpeedAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 6406691L);
	s->GetArpeggiatorSpeedAMSIntModSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorSpeedAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -70L);
	s->GetArpeggiatorSpeedAMSIntModIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorSpeedAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 2094874271L);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorSpeedAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2094874271L);
	s->GetArpeggiatorMIDITempoSync(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorMIDITempoSync value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetArpeggiatorMIDITempoSyncBaseNote(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorMIDITempoSyncBaseNote value", CSTGParamsOwner::sValueGetterTemp.value, 89L);
	s->GetArpeggiatorMIDITempoSyncTimes(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorMIDITempoSyncTimes value", CSTGParamsOwner::sValueGetterTemp.value, 248L);
	s->GetArpeggiatorMIDITempoSyncTimesAMSSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorMIDITempoSyncTimesAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 54L);
	s->GetArpeggiatorMIDITempoSyncTimesAMSIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorMIDITempoSyncTimesAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -105L);
	s->GetArpeggiatorMIDITempoSyncTimesAMSIntModSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorMIDITempoSyncTimesAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, 116L);
	s->GetArpeggiatorMIDITempoSyncTimesAMSIntModIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorMIDITempoSyncTimesAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -43L);
	s->GetArpeggiatorRange(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorRange value", CSTGParamsOwner::sValueGetterTemp.value, 19L);
	s->GetArpeggiatorRangeAMSSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorRangeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 81L);
	s->GetArpeggiatorRangeAMSIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorRangeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -78L);
	s->GetArpeggiatorMode(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorMode value", CSTGParamsOwner::sValueGetterTemp.value, -16L);
	s->GetArpeggiatorModeAMSSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorModeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 46L);
	s->GetArpeggiatorModeAMSIntensity(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorModeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -113L);
	s->GetArpeggiatorLatch(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorLatch value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetArpeggiatorLatchAMSSource(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorLatchAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -51L);
	s->GetArpeggiatorLatchAMSSwitchMode(ctx);
	check_eq("CSTGPolysixModelPatch::GetArpeggiatorLatchAMSSwitchMode value", CSTGParamsOwner::sValueGetterTemp.value, 108L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
