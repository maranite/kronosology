// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_common_effect_lfo_valuegetters.cpp  -  KAT for
 * CSTGCommonEffectLFO's Get* family -- all 8 real weak-symbol ctx-only
 * candidates, see ../src/engine/stg_common_effect_lfo_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own
 * disassembly-derived translation used -- not by re-using the .cpp
 * file's C output strings -- against the same deterministic non-trivial
 * byte pattern as the rest of the STG value-getter family's KATs:
 * buf[i] = i times 0x9f plus 0x37, all mod 0x100. This class has no
 * ctx-dynamic-index methods, so the ctx buffer's own contents are never
 * actually read.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_common_effect_lfo.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x100
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGCommonEffectLFO value-getter family known-answer test (8 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);

	CSTGCommonEffectLFO *s = (CSTGCommonEffectLFO *)g_buf;
	CSTGMessageContext &ctx = *(CSTGMessageContext *)g_ctxbuf;

	s->GetValueFrequency(ctx);
	check_eq("CSTGCommonEffectLFO::GetValueFrequency value", CSTGParamsOwner::sValueGetterTemp.value, -1863232845L);
	check_eq("CSTGCommonEffectLFO::GetValueFrequency displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1863232845L);
	s->GetValueTempo(ctx);
	check_eq("CSTGCommonEffectLFO::GetValueTempo value", CSTGParamsOwner::sValueGetterTemp.value, 208522799L);
	check_eq("CSTGCommonEffectLFO::GetValueTempo displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 208522799L);
	s->GetValueControlChannel(ctx);
	check_eq("CSTGCommonEffectLFO::GetValueControlChannel value", CSTGParamsOwner::sValueGetterTemp.value, 171L);
	s->GetValueResetSource(ctx);
	check_eq("CSTGCommonEffectLFO::GetValueResetSource value", CSTGParamsOwner::sValueGetterTemp.value, 74L);
	s->GetValueTempoTimes(ctx);
	check_eq("CSTGCommonEffectLFO::GetValueTempoTimes value", CSTGParamsOwner::sValueGetterTemp.value, 233L);
	s->GetValueTempoBaseNote(ctx);
	check_eq("CSTGCommonEffectLFO::GetValueTempoBaseNote value", CSTGParamsOwner::sValueGetterTemp.value, -120L);
	s->GetValueResetEnable(ctx);
	check_eq("CSTGCommonEffectLFO::GetValueResetEnable value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetValueTempoMIDISync(ctx);
	check_eq("CSTGCommonEffectLFO::GetValueTempoMIDISync value", CSTGParamsOwner::sValueGetterTemp.value, 1L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
