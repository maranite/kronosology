// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_tone_adjust_valuegetters.cpp  -  KAT for CSTGToneAdjust's
 * Get* family -- all 7 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_tone_adjust_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/ctx-index facts the source file's own
 * disassembly-derived translation used -- not by re-using the .cpp
 * file's C output strings -- against the same deterministic non-trivial
 * byte pattern as the rest of the STG value-getter family's KATs:
 * buf[i] = i times 0x9f plus 0x37, all mod 0x100, ctx index fixed at 3.
 *
 * This class is also the family's first confirmed case of a Get*
 * method with a genuine SIDE EFFECT on the context object -- three of
 * the seven methods below write back into ctx.changedFlag as well as
 * returning a value, both checked explicitly.
 */

#include <cstdio>
#include <cstring>
#include "oa_global.h"

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
	printf("CSTGToneAdjust value-getter family known-answer test (7 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(unsigned int *)(g_ctxbuf + 0x4) = 3;

	CSTGToneAdjust *s = (CSTGToneAdjust *)g_buf;
	CSTGToneAdjustMessageContext &ctx = *(CSTGToneAdjustMessageContext *)g_ctxbuf;

	s->GetValueAssignSlider(ctx);
	check_eq("CSTGToneAdjust::GetValueAssignSlider value", CSTGParamsOwner::sValueGetterTemp.value, 72L);
	check_eq("CSTGToneAdjust::GetValueAssignSlider ctx.changedFlag", ctx.changedFlag, 0L);
	s->GetValueAssignKnob(ctx);
	check_eq("CSTGToneAdjust::GetValueAssignKnob value", CSTGParamsOwner::sValueGetterTemp.value, 19L);
	check_eq("CSTGToneAdjust::GetValueAssignKnob ctx.changedFlag", ctx.changedFlag, 1L);
	s->GetValueAssignSwitch(ctx);
	check_eq("CSTGToneAdjust::GetValueAssignSwitch value", CSTGParamsOwner::sValueGetterTemp.value, 15L);
	check_eq("CSTGToneAdjust::GetValueAssignSwitch ctx.changedFlag", ctx.changedFlag, 1L);
	s->GetValueAssignSwitchOnValue(ctx);
	check_eq("CSTGToneAdjust::GetValueAssignSwitchOnValue value", CSTGParamsOwner::sValueGetterTemp.value, -29716L);
	s->GetValueSliderValue(ctx);
	check_eq("CSTGToneAdjust::GetValueSliderValue value", CSTGParamsOwner::sValueGetterTemp.value, 27596L);
	s->GetValueKnobValue(ctx);
	check_eq("CSTGToneAdjust::GetValueKnobValue value", CSTGParamsOwner::sValueGetterTemp.value, -26118L);
	s->GetValueSwitchValue(ctx);
	check_eq("CSTGToneAdjust::GetValueSwitchValue value (idx=3, bit clear)", CSTGParamsOwner::sValueGetterTemp.value, 0L);

	/* Re-exercise GetValueSwitchValue's ctx.index-as-shift-amount shape
	 * at a second index landing on a SET bit of the same fixed word
	 * field, so this genuinely new shape is confirmed on both a 0 and a
	 * 1 outcome, not just whichever bit idx=3 happens to land on. */
	*(unsigned int *)(g_ctxbuf + 0x4) = 4;
	s->GetValueSwitchValue(ctx);
	check_eq("CSTGToneAdjust::GetValueSwitchValue value (idx=4, bit set)", CSTGParamsOwner::sValueGetterTemp.value, 1L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
