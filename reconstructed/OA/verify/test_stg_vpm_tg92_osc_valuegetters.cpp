// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_vpm_tg92_osc_valuegetters.cpp  -  KAT for CSTGVPMTG92Osc's
 * Get* family -- all 9 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_vpm_tg92_osc_valuegetters.cpp.
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
#include "oa_stg_vpm_tg92_osc.h"

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
	printf("CSTGVPMTG92Osc value-getter family known-answer test (9 methods)\n");
	printf("===================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGVPMTG92Osc *s = (CSTGVPMTG92Osc *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetVolume(ctx);
	check_eq("CSTGVPMTG92Osc::GetVolume value", CSTGParamsOwner::sValueGetterTemp.value, -313610480L);
	check_eq("CSTGVPMTG92Osc::GetVolume displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -313610480L);
	s->GetVolumeEGSelect(ctx);
	check_eq("CSTGVPMTG92Osc::GetVolumeEGSelect value", CSTGParamsOwner::sValueGetterTemp.value, -116L);
	s->GetVolumeVelocitySensitivity(ctx);
	check_eq("CSTGVPMTG92Osc::GetVolumeVelocitySensitivity value", CSTGParamsOwner::sValueGetterTemp.value, 141150763L);
	check_eq("CSTGVPMTG92Osc::GetVolumeVelocitySensitivity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 141150763L);
	s->GetVolumeAMSSource(ctx);
	check_eq("CSTGVPMTG92Osc::GetVolumeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -97L);
	s->GetVolumeAMSIntensity(ctx);
	check_eq("CSTGVPMTG92Osc::GetVolumeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -2065348953L);
	check_eq("CSTGVPMTG92Osc::GetVolumeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2065348953L);
	s->GetVolumeAMSIntModSource(ctx);
	check_eq("CSTGVPMTG92Osc::GetVolumeAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, 62L);
	s->GetVolumeAMSIntModIntensity(ctx);
	check_eq("CSTGVPMTG92Osc::GetVolumeAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 6406691L);
	check_eq("CSTGVPMTG92Osc::GetVolumeAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 6406691L);
	s->GetVolumeAMSMode(ctx);
	check_eq("CSTGVPMTG92Osc::GetVolumeAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, -35L);
	s->GetOscOnOff(ctx);
	check_eq("CSTGVPMTG92Osc::GetOscOnOff value", CSTGParamsOwner::sValueGetterTemp.value, 0L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
