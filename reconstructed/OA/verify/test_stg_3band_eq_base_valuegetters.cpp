// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_3band_eq_base_valuegetters.cpp  -  KAT for
 * CSTG3BandEQBase's Get* family -- all 6 real weak-symbol ctx-only
 * candidates, see ../src/engine/stg_3band_eq_base_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own
 * decoder used -- not by re-using the .cpp file's C output strings --
 * against the same deterministic non-trivial byte pattern as the rest
 * of the STG value-getter family's KATs: buf[i] = i times 0x9f plus
 * 0x37, all mod 0x100.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_3band_eq_base.h"

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
	printf("CSTG3BandEQBase value-getter family known-answer test (6 methods)\n");
	printf("============================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);

	CSTG3BandEQBase *s = (CSTG3BandEQBase *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetBypassValue(ctx);
	check_eq("CSTG3BandEQBase::GetBypassValue value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetInputTrimValue(ctx);
	check_eq("CSTG3BandEQBase::GetInputTrimValue value", CSTGParamsOwner::sValueGetterTemp.value, -1997976917L);
	check_eq("CSTG3BandEQBase::GetInputTrimValue displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1997976917L);
	s->GetLowGainValue(ctx);
	check_eq("CSTG3BandEQBase::GetLowGainValue value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTG3BandEQBase::GetLowGainValue displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);
	s->GetMidFreqValue(ctx);
	check_eq("CSTG3BandEQBase::GetMidFreqValue value", CSTGParamsOwner::sValueGetterTemp.value, 2027502235L);
	check_eq("CSTG3BandEQBase::GetMidFreqValue displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2027502235L);
	s->GetMidGainValue(ctx);
	check_eq("CSTG3BandEQBase::GetMidGainValue value", CSTGParamsOwner::sValueGetterTemp.value, -60965345L);
	check_eq("CSTG3BandEQBase::GetMidGainValue displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -60965345L);
	s->GetHighGainValue(ctx);
	check_eq("CSTG3BandEQBase::GetHighGainValue value", CSTGParamsOwner::sValueGetterTemp.value, -2132720989L);
	check_eq("CSTG3BandEQBase::GetHighGainValue displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2132720989L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
