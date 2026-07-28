// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_analog4pole_valuegetters.cpp  -  KAT for CSTGAnalog4Pole's Get*
 * family -- all 7 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_analog4pole_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own decoder
 * used -- not by re-using the .cpp file's C output strings -- against the
 * same deterministic non-trivial byte pattern as the rest of the STG
 * value-getter family's KATs: buf[i] = i times 0x9f plus 0x37, all mod
 * 0x100. This class has no ctx-dynamic-index methods, so ctx's own fields
 * are never read, but the buffer is still filled the same way for
 * consistency with the family's KAT convention. Buffer is sized past this
 * class's own largest field offset (+0x12c) since its struct layout is
 * unusually large.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_analog4pole.h"

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
	printf("CSTGAnalog4Pole value-getter family known-answer test (7 methods)\n");
	printf("============================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGAnalog4Pole *s = (CSTGAnalog4Pole *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetRoutingValue(ctx);
	check_eq("CSTGAnalog4Pole::GetRoutingValue value", CSTGParamsOwner::sValueGetterTemp.value, 120L);
	s->GetFilterAPan(ctx);
	check_eq("CSTGAnalog4Pole::GetFilterAPan value", CSTGParamsOwner::sValueGetterTemp.value, -650470404L);
	check_eq("CSTGAnalog4Pole::GetFilterAPan displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -650470404L);
	s->GetFilterAPanAMSSource(ctx);
	check_eq("CSTGAnalog4Pole::GetFilterAPanAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -12L);
	s->GetFilterAPanAMSIntensity(ctx);
	check_eq("CSTGAnalog4Pole::GetFilterAPanAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1437996920L);
	check_eq("CSTGAnalog4Pole::GetFilterAPanAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1437996920L);
	s->GetFilterBPan(ctx);
	check_eq("CSTGAnalog4Pole::GetFilterBPan value", CSTGParamsOwner::sValueGetterTemp.value, 1892758163L);
	check_eq("CSTGAnalog4Pole::GetFilterBPan displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1892758163L);
	s->GetFilterBPanAMSSource(ctx);
	check_eq("CSTGAnalog4Pole::GetFilterBPanAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -117L);
	s->GetFilterBPanAMSIntensity(ctx);
	check_eq("CSTGAnalog4Pole::GetFilterBPanAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -330453489L);
	check_eq("CSTGAnalog4Pole::GetFilterBPanAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -330453489L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
