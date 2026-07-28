// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_step_seq_valuegetters.cpp  -  KAT for
 * CSTGStepSeq's Get* family -- 14 real weak-symbol
 * ctx-only candidates, see ../src/engine/stg_step_seq_valuegetters.cpp.
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
#include "oa_stg_step_seq.h"

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
	printf("CSTGStepSeq value-getter family known-answer test (14 methods)\n");
	printf("==================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGStepSeq *s = (CSTGStepSeq *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetAMSResetSource(ctx);
	check_eq("CSTGStepSeq::GetAMSResetSource value", CSTGParamsOwner::sValueGetterTemp.value, -109L);
	s->GetAttack(ctx);
	check_eq("CSTGStepSeq::GetAttack value", CSTGParamsOwner::sValueGetterTemp.value, -60965345L);
	check_eq("CSTGStepSeq::GetAttack displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -60965345L);
	s->GetDecay(ctx);
	check_eq("CSTGStepSeq::GetDecay value", CSTGParamsOwner::sValueGetterTemp.value, 2027502235L);
	check_eq("CSTGStepSeq::GetDecay displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2027502235L);
	s->GetEndStep(ctx);
	check_eq("CSTGStepSeq::GetEndStep value", CSTGParamsOwner::sValueGetterTemp.value, 101L);
	s->GetKeySync(ctx);
	check_eq("CSTGStepSeq::GetKeySync value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetOneShot(ctx);
	check_eq("CSTGStepSeq::GetOneShot value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetResetThreshold(ctx);
	check_eq("CSTGStepSeq::GetResetThreshold value", CSTGParamsOwner::sValueGetterTemp.value, -195709417L);
	check_eq("CSTGStepSeq::GetResetThreshold displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -195709417L);
	s->GetStartStep(ctx);
	check_eq("CSTGStepSeq::GetStartStep value", CSTGParamsOwner::sValueGetterTemp.value, 198L);
	s->GetStartStepAMSIntensity(ctx);
	check_eq("CSTGStepSeq::GetStartStepAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -515726588L);
	check_eq("CSTGStepSeq::GetStartStepAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -515726588L);
	s->GetStartStepAMSSource(ctx);
	check_eq("CSTGStepSeq::GetStartStepAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -128L);
	s->GetStepDuration(ctx);
	check_eq("CSTGStepSeq::GetStepDuration value", CSTGParamsOwner::sValueGetterTemp.value, -114L);
	s->GetStepTimes(ctx);
	check_eq("CSTGStepSeq::GetStepTimes value", CSTGParamsOwner::sValueGetterTemp.value, 110L);
	s->GetStepValue(ctx);
	check_eq("CSTGStepSeq::GetStepValue value", CSTGParamsOwner::sValueGetterTemp.value, -82L);
	s->GetValueAMSSource(ctx);
	check_eq("CSTGStepSeq::GetValueAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 50L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}