// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_ams_mixer_base_valuegetters.cpp  -  KAT for
 * CSTGAMSMixerBase's Get* family -- 17 real weak-symbol
 * ctx-only candidates, see ../src/engine/stg_ams_mixer_base_valuegetters.cpp.
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
#include "oa_stg_ams_mixer_base.h"

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
	printf("CSTGAMSMixerBase value-getter family known-answer test (17 methods)\n");
	printf("=======================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGAMSMixerBase *s = (CSTGAMSMixerBase *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetAAmount(ctx);
	check_eq("CSTGAMSMixerBase::GetAAmount value", CSTGParamsOwner::sValueGetterTemp.value, -1997976917L);
	check_eq("CSTGAMSMixerBase::GetAAmount displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1997976917L);
	s->GetASource(ctx);
	check_eq("CSTGAMSMixerBase::GetASource value", CSTGParamsOwner::sValueGetterTemp.value, -98L);
	s->GetAmountForOffset(ctx);
	check_eq("CSTGAMSMixerBase::GetAmountForOffset value", CSTGParamsOwner::sValueGetterTemp.value, -2132720989L);
	check_eq("CSTGAMSMixerBase::GetAmountForOffset displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2132720989L);
	s->GetBAmount(ctx);
	check_eq("CSTGAMSMixerBase::GetBAmount value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTGAMSMixerBase::GetBAmount displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);
	s->GetBSource(ctx);
	check_eq("CSTGAMSMixerBase::GetBSource value", CSTGParamsOwner::sValueGetterTemp.value, 61L);
	s->GetGateAtAboveThresholdFixedValue(ctx);
	check_eq("CSTGAMSMixerBase::GetGateAtAboveThresholdFixedValue value", CSTGParamsOwner::sValueGetterTemp.value, 1623270019L);
	check_eq("CSTGAMSMixerBase::GetGateAtAboveThresholdFixedValue displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1623270019L);
	s->GetGateAtAboveThresholdUseAMS(ctx);
	check_eq("CSTGAMSMixerBase::GetGateAtAboveThresholdUseAMS value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetGateAtNoteOnOnly(ctx);
	check_eq("CSTGAMSMixerBase::GetGateAtNoteOnOnly value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetGateBelowThresholdFixedValue(ctx);
	check_eq("CSTGAMSMixerBase::GetGateBelowThresholdFixedValue value", CSTGParamsOwner::sValueGetterTemp.value, -465197561L);
	check_eq("CSTGAMSMixerBase::GetGateBelowThresholdFixedValue displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -465197561L);
	s->GetGateBelowThresholdUseAMS(ctx);
	check_eq("CSTGAMSMixerBase::GetGateBelowThresholdUseAMS value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetGateControlAMSSourceSelect(ctx);
	check_eq("CSTGAMSMixerBase::GetGateControlAMSSourceSelect value", CSTGParamsOwner::sValueGetterTemp.value, -36L);
	s->GetGateThreshold(ctx);
	check_eq("CSTGAMSMixerBase::GetGateThreshold value", CSTGParamsOwner::sValueGetterTemp.value, 1758014091L);
	check_eq("CSTGAMSMixerBase::GetGateThreshold displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1758014091L);
	s->GetMixerType(ctx);
	check_eq("CSTGAMSMixerBase::GetMixerType value", CSTGParamsOwner::sValueGetterTemp.value, -1L);
	s->GetOffset(ctx);
	check_eq("CSTGAMSMixerBase::GetOffset value", CSTGParamsOwner::sValueGetterTemp.value, -60965345L);
	check_eq("CSTGAMSMixerBase::GetOffset displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -60965345L);
	s->GetQuantizeSteps(ctx);
	check_eq("CSTGAMSMixerBase::GetQuantizeSteps value", CSTGParamsOwner::sValueGetterTemp.value, -330453489L);
	check_eq("CSTGAMSMixerBase::GetQuantizeSteps displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -330453489L);
	s->GetShape(ctx);
	check_eq("CSTGAMSMixerBase::GetShape value", CSTGParamsOwner::sValueGetterTemp.value, 1892758163L);
	check_eq("CSTGAMSMixerBase::GetShape displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1892758163L);
	s->GetShapeType(ctx);
	check_eq("CSTGAMSMixerBase::GetShapeType value", CSTGParamsOwner::sValueGetterTemp.value, 1L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}