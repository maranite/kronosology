// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_vpm_pitch_mod_tg92osc_valuegetters.cpp  -  KAT for
 * CSTGVPMPitchModTG92Osc's Get* family -- all 5 real weak-symbol
 * ctx-only candidates, see
 * ../src/engine/stg_vpm_pitch_mod_tg92osc_valuegetters.cpp.
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
#include "oa_stg_vpm_pitch_mod_tg92osc.h"

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
	printf("CSTGVPMPitchModTG92Osc value-getter family known-answer test (5 methods)\n");
	printf("==========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGVPMPitchModTG92Osc *s = (CSTGVPMPitchModTG92Osc *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetAMSSource(ctx);
	check_eq("CSTGVPMPitchModTG92Osc::GetAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 50L);
	s->GetAMSIntensity(ctx);
	check_eq("CSTGVPMPitchModTG92Osc::GetAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1812703818L);
	check_eq("CSTGVPMPitchModTG92Osc::GetAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1812703818L);
	s->GetAMSIntensityAMSSource(ctx);
	check_eq("CSTGVPMPitchModTG92Osc::GetAMSIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 23L);
	s->GetAMSIntensityAMSIntensity(ctx);
	check_eq("CSTGVPMPitchModTG92Osc::GetAMSIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 2027502235L);
	check_eq("CSTGVPMPitchModTG92Osc::GetAMSIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2027502235L);
	s->GetUseCommonMod(ctx);
	check_eq("CSTGVPMPitchModTG92Osc::GetUseCommonMod value", CSTGParamsOwner::sValueGetterTemp.value, 0L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
