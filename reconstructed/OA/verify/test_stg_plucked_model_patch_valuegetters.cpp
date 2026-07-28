// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_plucked_model_patch_valuegetters.cpp  -  KAT for
 * CSTGPluckedModelPatch's Get* family -- all 6 real weak-symbol ctx-only
 * candidates, see ../src/engine/stg_plucked_model_patch_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own decoder
 * used -- not by re-using the .cpp file's C output strings -- against the
 * same deterministic non-trivial byte pattern as the rest of the STG
 * value-getter family's KATs: buf[i] = i times 0x9f plus 0x37, all mod
 * 0x100. This class has no ctx-dynamic-index methods, so ctx's own fields
 * are never read, but the buffer is still filled the same way for
 * consistency with the family's KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_plucked_model_patch.h"

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
	printf("CSTGPluckedModelPatch value-getter family known-answer test (6 methods)\n");
	printf("============================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGPluckedModelPatch *s = (CSTGPluckedModelPatch *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetFeedbackDistance(ctx);
	check_eq("CSTGPluckedModelPatch::GetFeedbackDistance value", CSTGParamsOwner::sValueGetterTemp.value, -1997976917L);
	check_eq("CSTGPluckedModelPatch::GetFeedbackDistance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1997976917L);
	s->GetFeedbackDistanceAMSSource(ctx);
	check_eq("CSTGPluckedModelPatch::GetFeedbackDistanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -93L);
	s->GetFeedbackDistanceAMSIntensity(ctx);
	check_eq("CSTGPluckedModelPatch::GetFeedbackDistanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTGPluckedModelPatch::GetFeedbackDistanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);
	s->GetFeedbackOrientation(ctx);
	check_eq("CSTGPluckedModelPatch::GetFeedbackOrientation value", CSTGParamsOwner::sValueGetterTemp.value, -60965345L);
	check_eq("CSTGPluckedModelPatch::GetFeedbackOrientation displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -60965345L);
	s->GetFeedbackOrientationAMSSource(ctx);
	check_eq("CSTGPluckedModelPatch::GetFeedbackOrientationAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 23L);
	s->GetFeedbackOrientationAMSIntensity(ctx);
	check_eq("CSTGPluckedModelPatch::GetFeedbackOrientationAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 2027502235L);
	check_eq("CSTGPluckedModelPatch::GetFeedbackOrientationAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2027502235L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
