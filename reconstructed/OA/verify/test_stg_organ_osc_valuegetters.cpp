// SPDX-License-Identifier: GPL-2.0
#include <cstdio>
#include <cstring>
#include "oa_stg_organ_osc.h"

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
	printf("CSTGOrganOsc value-getter family known-answer test (13 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGOrganOsc *s = (CSTGOrganOsc *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetUpDrawbar(ctx);
	check_eq("CSTGOrganOsc::GetUpDrawbar value", CSTGParamsOwner::sValueGetterTemp.value, 136L);
	s->GetLowDrawbar(ctx);
	check_eq("CSTGOrganOsc::GetLowDrawbar value", CSTGParamsOwner::sValueGetterTemp.value, 155L);
	s->GetDrawbarLevelCurve(ctx);
	check_eq("CSTGOrganOsc::GetDrawbarLevelCurve value", CSTGParamsOwner::sValueGetterTemp.value, 236L);
	s->GetSplitPoint(ctx);
	check_eq("CSTGOrganOsc::GetSplitPoint value", CSTGParamsOwner::sValueGetterTemp.value, 139L);
	s->GetUpperOctaveShift(ctx);
	check_eq("CSTGOrganOsc::GetUpperOctaveShift value", CSTGParamsOwner::sValueGetterTemp.value, 42L);
	s->GetLowerOctaveShift(ctx);
	check_eq("CSTGOrganOsc::GetLowerOctaveShift value", CSTGParamsOwner::sValueGetterTemp.value, -55L);
	s->GetEXDrawbarModeUp(ctx);
	check_eq("CSTGOrganOsc::GetEXDrawbarModeUp value", CSTGParamsOwner::sValueGetterTemp.value, 104L);
	s->GetEXDrawbarModeLow(ctx);
	check_eq("CSTGOrganOsc::GetEXDrawbarModeLow value", CSTGParamsOwner::sValueGetterTemp.value, 7L);
	s->GetEXDrawbarPitchUp(ctx);
	check_eq("CSTGOrganOsc::GetEXDrawbarPitchUp value", CSTGParamsOwner::sValueGetterTemp.value, 131L);
	s->GetEXDrawbarPitchLow(ctx);
	check_eq("CSTGOrganOsc::GetEXDrawbarPitchLow value", CSTGParamsOwner::sValueGetterTemp.value, 255L);
	s->GetEXPercDrawbar(ctx);
	check_eq("CSTGOrganOsc::GetEXPercDrawbar value", CSTGParamsOwner::sValueGetterTemp.value, 174L);
	s->GetPercAssignValue(ctx);
	check_eq("CSTGOrganOsc::GetPercAssignValue value", CSTGParamsOwner::sValueGetterTemp.value, -98L);
	s->GetEnvelopeType(ctx);
	check_eq("CSTGOrganOsc::GetEnvelopeType value", CSTGParamsOwner::sValueGetterTemp.value, 61L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
