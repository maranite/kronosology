// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_ep_model_patch_valuegetters.cpp  -  KAT for CSTGEPModelPatch's
 * Get* family -- 42 of 42 real weak-symbol candidates, see
 * ../src/engine/stg_ep_model_patch_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own
 * decoder used -- not by re-using the .cpp file's C output strings --
 * against the same deterministic non-trivial byte pattern as the rest of
 * the STG value-getter family's KATs: buf[i] = i times 0x9f plus 0x37,
 * all mod 0x100.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_ep_model_patch.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld\n", label, got); return; }
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x500
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGEPModelPatch value-getter family known-answer test (42 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGEPModelPatch *s = (CSTGEPModelPatch *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetModelType(ctx);
	check_eq("CSTGEPModelPatch::GetModelType value", CSTGParamsOwner::sValueGetterTemp.value, -85L);
	s->GetIFXTypeParam(ctx);
	check_eq("CSTGEPModelPatch::GetIFXTypeParam value", CSTGParamsOwner::sValueGetterTemp.value, -45L);
	s->GetTinePreAmpVolume(ctx);
	check_eq("CSTGEPModelPatch::GetTinePreAmpVolume value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTGEPModelPatch::GetTinePreAmpVolume displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);
	s->GetTinePreAmpVolumeAMSSource(ctx);
	check_eq("CSTGEPModelPatch::GetTinePreAmpVolumeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 31L);
	s->GetTinePreAmpVolumeAMSIntensity(ctx);
	check_eq("CSTGEPModelPatch::GetTinePreAmpVolumeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -2132720989L);
	check_eq("CSTGEPModelPatch::GetTinePreAmpVolumeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2132720989L);
	s->GetTineTreble(ctx);
	check_eq("CSTGEPModelPatch::GetTineTreble value", CSTGParamsOwner::sValueGetterTemp.value, 2027502235L);
	check_eq("CSTGEPModelPatch::GetTineTreble displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2027502235L);
	s->GetTineTrebleAMSSource(ctx);
	check_eq("CSTGEPModelPatch::GetTineTrebleAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -109L);
	s->GetTineTrebleAMSIntensity(ctx);
	check_eq("CSTGEPModelPatch::GetTineTrebleAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -195709417L);
	check_eq("CSTGEPModelPatch::GetTineTrebleAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -195709417L);
	s->GetTineBass(ctx);
	check_eq("CSTGEPModelPatch::GetTineBass value", CSTGParamsOwner::sValueGetterTemp.value, -330453489L);
	check_eq("CSTGEPModelPatch::GetTineBass displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -330453489L);
	s->GetTineBassAMSSource(ctx);
	check_eq("CSTGEPModelPatch::GetTineBassAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 7L);
	s->GetTineBassAMSIntensity(ctx);
	check_eq("CSTGEPModelPatch::GetTineBassAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1758014091L);
	check_eq("CSTGEPModelPatch::GetTineBassAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1758014091L);
	s->GetTineTremoloOnOff(ctx);
	check_eq("CSTGEPModelPatch::GetTineTremoloOnOff value", CSTGParamsOwner::sValueGetterTemp.value, 166L);
	s->GetTineTremoloOnOffAMSSource(ctx);
	check_eq("CSTGEPModelPatch::GetTineTremoloOnOffAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 69L);
	s->GetTineTremoloOnOffAMSMode(ctx);
	check_eq("CSTGEPModelPatch::GetTineTremoloOnOffAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, -28L);
	s->GetTineTremoloSpeed(ctx);
	check_eq("CSTGEPModelPatch::GetTineTremoloSpeed value", CSTGParamsOwner::sValueGetterTemp.value, 1623270019L);
	check_eq("CSTGEPModelPatch::GetTineTremoloSpeed displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1623270019L);
	s->GetTineTremoloSpeedAMSSource(ctx);
	check_eq("CSTGEPModelPatch::GetTineTremoloSpeedAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 123L);
	s->GetTineTremoloSpeedAMSIntensity(ctx);
	check_eq("CSTGEPModelPatch::GetTineTremoloSpeedAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -599941377L);
	check_eq("CSTGEPModelPatch::GetTineTremoloSpeedAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -599941377L);
	s->GetTineTremoloIntensity(ctx);
	check_eq("CSTGEPModelPatch::GetTineTremoloIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -734685449L);
	check_eq("CSTGEPModelPatch::GetTineTremoloIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -734685449L);
	s->GetTineTremoloIntensityAMSSource(ctx);
	check_eq("CSTGEPModelPatch::GetTineTremoloIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -17L);
	s->GetTineTremoloIntensityAMSIntensity(ctx);
	check_eq("CSTGEPModelPatch::GetTineTremoloIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1353781875L);
	check_eq("CSTGEPModelPatch::GetTineTremoloIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1353781875L);
	s->GetTineCabinetOnOff(ctx);
	check_eq("CSTGEPModelPatch::GetTineCabinetOnOff value", CSTGParamsOwner::sValueGetterTemp.value, 142L);
	s->GetTineCabinetOnOffAMSSource(ctx);
	check_eq("CSTGEPModelPatch::GetTineCabinetOnOffAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 45L);
	s->GetTineCabinetOnOffAMSMode(ctx);
	check_eq("CSTGEPModelPatch::GetTineCabinetOnOffAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, -52L);
	s->GetTineCabinetDrive(ctx);
	check_eq("CSTGEPModelPatch::GetTineCabinetDrive value", CSTGParamsOwner::sValueGetterTemp.value, 1219037803L);
	check_eq("CSTGEPModelPatch::GetTineCabinetDrive displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1219037803L);
	s->GetTineCabinetDriveAMSSource(ctx);
	check_eq("CSTGEPModelPatch::GetTineCabinetDriveAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 99L);
	s->GetTineCabinetDriveAMSIntensity(ctx);
	check_eq("CSTGEPModelPatch::GetTineCabinetDriveAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1004173593L);
	check_eq("CSTGEPModelPatch::GetTineCabinetDriveAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1004173593L);
	s->GetReedVolume(ctx);
	check_eq("CSTGEPModelPatch::GetReedVolume value", CSTGParamsOwner::sValueGetterTemp.value, -1138917665L);
	check_eq("CSTGEPModelPatch::GetReedVolume displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1138917665L);
	s->GetReedVolumeAMSSource(ctx);
	check_eq("CSTGEPModelPatch::GetReedVolumeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -41L);
	s->GetReedVolumeAMSIntensity(ctx);
	check_eq("CSTGEPModelPatch::GetReedVolumeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 949615195L);
	check_eq("CSTGEPModelPatch::GetReedVolumeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 949615195L);
	s->GetReedVibrato(ctx);
	check_eq("CSTGEPModelPatch::GetReedVibrato value", CSTGParamsOwner::sValueGetterTemp.value, 814871123L);
	check_eq("CSTGEPModelPatch::GetReedVibrato displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 814871123L);
	s->GetReedVibratoAMSSource(ctx);
	check_eq("CSTGEPModelPatch::GetReedVibratoAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 75L);
	s->GetReedVibratoAMSIntensity(ctx);
	check_eq("CSTGEPModelPatch::GetReedVibratoAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1408405809L);
	check_eq("CSTGEPModelPatch::GetReedVibratoAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1408405809L);
	s->GetReedSpeed(ctx);
	check_eq("CSTGEPModelPatch::GetReedSpeed value", CSTGParamsOwner::sValueGetterTemp.value, -1543149881L);
	check_eq("CSTGEPModelPatch::GetReedSpeed displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1543149881L);
	s->GetReedSpeedAMSSource(ctx);
	check_eq("CSTGEPModelPatch::GetReedSpeedAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -65L);
	s->GetReedSpeedAMSIntensity(ctx);
	check_eq("CSTGEPModelPatch::GetReedSpeedAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 545382979L);
	check_eq("CSTGEPModelPatch::GetReedSpeedAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 545382979L);
	s->GetReedCabinetOnOff(ctx);
	check_eq("CSTGEPModelPatch::GetReedCabinetOnOff value", CSTGParamsOwner::sValueGetterTemp.value, 94L);
	s->GetReedCabinetOnOffAMSSource(ctx);
	check_eq("CSTGEPModelPatch::GetReedCabinetOnOffAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -3L);
	s->GetReedCabinetOnOffAMSMode(ctx);
	check_eq("CSTGEPModelPatch::GetReedCabinetOnOffAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, -100L);
	s->GetReedCabinetDrive(ctx);
	check_eq("CSTGEPModelPatch::GetReedCabinetDrive value", CSTGParamsOwner::sValueGetterTemp.value, 410638907L);
	check_eq("CSTGEPModelPatch::GetReedCabinetDrive displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 410638907L);
	s->GetReedCabinetDriveAMSSource(ctx);
	check_eq("CSTGEPModelPatch::GetReedCabinetDriveAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 51L);
	s->GetReedCabinetDriveAMSIntensity(ctx);
	check_eq("CSTGEPModelPatch::GetReedCabinetDriveAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1795860809L);
	check_eq("CSTGEPModelPatch::GetReedCabinetDriveAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1795860809L);
	s->GetIFXEnable(ctx);
	check_eq("CSTGEPModelPatch::GetIFXEnable value", CSTGParamsOwner::sValueGetterTemp.value, 171L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
