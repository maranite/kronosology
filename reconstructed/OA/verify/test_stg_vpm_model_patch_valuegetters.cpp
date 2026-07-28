// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_vpm_model_patch_valuegetters.cpp  -  KAT for
 * CSTGVPMModelPatch's Get* family -- all 10 real weak-symbol ctx-only
 * candidates, see ../src/engine/stg_vpm_model_patch_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual/ctx-index/ctx-shift facts the
 * source file's own decoder used -- not by re-using the .cpp file's C
 * output strings -- against the same deterministic non-trivial byte
 * pattern as the rest of the STG value-getter family's KATs: buf[i] = i
 * times 0x9f plus 0x37, all mod 0x100. ctx's own dynamic-index field at
 * +0x4 is fixed at 3, matching the established KAT convention -- for
 * this class it is read both as a plain array index (GetInputJack) and
 * as a variable shift count masked to 5 bits (GetInterMixerLink/
 * GetOscMacroClass).
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_vpm_model_patch.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x1000
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGVPMModelPatch value-getter family known-answer test (10 methods)\n");
	printf("=======================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGVPMModelPatch *s = (CSTGVPMModelPatch *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetAlgorithm(ctx);
	check_eq("CSTGVPMModelPatch::GetAlgorithm value", CSTGParamsOwner::sValueGetterTemp.value, 55L);
	s->GetInputJack(ctx);
	check_eq("CSTGVPMModelPatch::GetInputJack value", CSTGParamsOwner::sValueGetterTemp.value, -77L);
	s->GetAnalog(ctx);
	check_eq("CSTGVPMModelPatch::GetAnalog value", CSTGParamsOwner::sValueGetterTemp.value, 747499087L);
	check_eq("CSTGVPMModelPatch::GetAnalog displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 747499087L);
	s->GetInterMixerLink(ctx);
	check_eq("CSTGVPMModelPatch::GetInterMixerLink value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetMacroBrightness(ctx);
	check_eq("CSTGVPMModelPatch::GetMacroBrightness value", CSTGParamsOwner::sValueGetterTemp.value, -1475777845L);
	check_eq("CSTGVPMModelPatch::GetMacroBrightness displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1475777845L);
	s->GetMacroBrightnessVelocitySensitivity(ctx);
	check_eq("CSTGVPMModelPatch::GetMacroBrightnessVelocitySensitivity value", CSTGParamsOwner::sValueGetterTemp.value, 612755015L);
	check_eq("CSTGVPMModelPatch::GetMacroBrightnessVelocitySensitivity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 612755015L);
	s->GetMacroTimbre(ctx);
	check_eq("CSTGVPMModelPatch::GetMacroTimbre value", CSTGParamsOwner::sValueGetterTemp.value, -1610521917L);
	check_eq("CSTGVPMModelPatch::GetMacroTimbre displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1610521917L);
	s->GetMacroFeedback(ctx);
	check_eq("CSTGVPMModelPatch::GetMacroFeedback value", CSTGParamsOwner::sValueGetterTemp.value, 478010943L);
	check_eq("CSTGVPMModelPatch::GetMacroFeedback displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 478010943L);
	s->GetMacroDetune(ctx);
	check_eq("CSTGVPMModelPatch::GetMacroDetune value", CSTGParamsOwner::sValueGetterTemp.value, -1728488773L);
	check_eq("CSTGVPMModelPatch::GetMacroDetune displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1728488773L);
	s->GetOscMacroClass(ctx);
	check_eq("CSTGVPMModelPatch::GetOscMacroClass value", CSTGParamsOwner::sValueGetterTemp.value, 1L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
