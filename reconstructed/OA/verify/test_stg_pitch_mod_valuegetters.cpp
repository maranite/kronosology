// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_pitch_mod_valuegetters.cpp  -  KAT for
 * CSTGPitchMod's Get* family -- 12 real weak-symbol
 * ctx-only candidates, see ../src/engine/stg_pitch_mod_valuegetters.cpp.
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
#include "oa_stg_pitch_mod.h"

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
	printf("CSTGPitchMod value-getter family known-answer test (12 methods)\n");
	printf("===================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGPitchMod *s = (CSTGPitchMod *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetEGAMSIntensity(ctx);
	check_eq("CSTGPitchMod::GetEGAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -279924462L);
	check_eq("CSTGPitchMod::GetEGAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -279924462L);
	s->GetEGAMSSource(ctx);
	check_eq("CSTGPitchMod::GetEGAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -114L);
	s->GetEGAmount(ctx);
	check_eq("CSTGPitchMod::GetEGAmount value", CSTGParamsOwner::sValueGetterTemp.value, 1758014091L);
	check_eq("CSTGPitchMod::GetEGAmount displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1758014091L);
	s->GetJSYToLFOAmount(ctx);
	check_eq("CSTGPitchMod::GetJSYToLFOAmount value", CSTGParamsOwner::sValueGetterTemp.value, 1353781875L);
	check_eq("CSTGPitchMod::GetJSYToLFOAmount displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1353781875L);
	s->GetLFOAMSIntensity(ctx);
	check_eq("CSTGPitchMod::GetLFOAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1539054974L);
	check_eq("CSTGPitchMod::GetLFOAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1539054974L);
	s->GetLFOAMSSource(ctx);
	check_eq("CSTGPitchMod::GetLFOAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -6L);
	s->GetLFOAmount(ctx);
	check_eq("CSTGPitchMod::GetLFOAmount value", CSTGParamsOwner::sValueGetterTemp.value, 1488525947L);
	check_eq("CSTGPitchMod::GetLFOAmount displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1488525947L);
	s->GetOctave(ctx);
	check_eq("CSTGPitchMod::GetOctave value", CSTGParamsOwner::sValueGetterTemp.value, 112L);
	s->GetPitchAMSIntensity(ctx);
	check_eq("CSTGPitchMod::GetPitchAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -734685449L);
	check_eq("CSTGPitchMod::GetPitchAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -734685449L);
	s->GetPitchAMSSource(ctx);
	check_eq("CSTGPitchMod::GetPitchAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 115L);
	s->GetTranspose(ctx);
	check_eq("CSTGPitchMod::GetTranspose value", CSTGParamsOwner::sValueGetterTemp.value, -47L);
	s->GetTune(ctx);
	check_eq("CSTGPitchMod::GetTune value", CSTGParamsOwner::sValueGetterTemp.value, -330453489L);
	check_eq("CSTGPitchMod::GetTune displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -330453489L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}