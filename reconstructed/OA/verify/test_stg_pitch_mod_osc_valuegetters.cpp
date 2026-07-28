// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_pitch_mod_osc_valuegetters.cpp  -  KAT for CSTGPitchModOsc's
 * Get* family -- all 8 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_pitch_mod_osc_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual/stride facts the source file's own
 * decoder used -- not by re-using the .cpp file's C output strings --
 * against the same deterministic non-trivial byte pattern as the rest of
 * the STG value-getter family's KATs: buf[i] = i times 0x9f plus 0x37, all
 * mod 0x100, with ctx's own dynamic-index field at +0x4 fixed at 3, same
 * as every other ctx-indexed class's own KAT in this family.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_pitch_mod_osc.h"

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
	printf("CSTGPitchModOsc value-getter family known-answer test (8 methods)\n");
	printf("============================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGPitchModOsc *s = (CSTGPitchModOsc *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetEGSelect(ctx);
	check_eq("CSTGPitchModOsc::GetEGSelect value", CSTGParamsOwner::sValueGetterTemp.value, 101L);
	s->GetEGAmount(ctx);
	check_eq("CSTGPitchModOsc::GetEGAmount value", CSTGParamsOwner::sValueGetterTemp.value, -515726588L);
	check_eq("CSTGPitchModOsc::GetEGAmount displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -515726588L);
	s->GetEGAMSSource(ctx);
	check_eq("CSTGPitchModOsc::GetEGAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -4L);
	s->GetEGAMSIntensity(ctx);
	check_eq("CSTGPitchModOsc::GetEGAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1572740992L);
	check_eq("CSTGPitchModOsc::GetEGAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1572740992L);
	s->GetAMSSource(ctx);
	check_eq("CSTGPitchModOsc::GetAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 104L);
	s->GetAMSIntensity(ctx);
	check_eq("CSTGPitchModOsc::GetAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -919958548L);
	check_eq("CSTGPitchModOsc::GetAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -919958548L);
	s->GetAMSIntensityAMSSource(ctx);
	check_eq("CSTGPitchModOsc::GetAMSIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -98L);
	s->GetAMSIntensityAMSIntensity(ctx);
	check_eq("CSTGPitchModOsc::GetAMSIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -10436318L);
	check_eq("CSTGPitchModOsc::GetAMSIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -10436318L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
