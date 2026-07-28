// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_simple_ams_mixer_valuegetters.cpp  -  KAT for
 * CSTGSimpleAMSMixer's Get* family -- all 5 real weak-symbol ctx-only
 * candidates, see ../src/engine/stg_simple_ams_mixer_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own
 * decoder used -- not by re-using the .cpp file's C output strings --
 * against the same deterministic non-trivial byte pattern as the rest of
 * the STG value-getter family's KATs: buf[i] = i times 0x9f plus 0x37,
 * all mod 0x100. ctx's own dynamic-index field at +0x4 is fixed at 3,
 * matching the established KAT convention, though this class has no
 * ctx-indexed methods to exercise it.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_simple_ams_mixer.h"

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
	printf("CSTGSimpleAMSMixer value-getter family known-answer test (5 methods)\n");
	printf("======================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGSimpleAMSMixer *s = (CSTGSimpleAMSMixer *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetType(ctx);
	check_eq("CSTGSimpleAMSMixer::GetType value", CSTGParamsOwner::sValueGetterTemp.value, -85L);
	s->GetSourceA(ctx);
	check_eq("CSTGSimpleAMSMixer::GetSourceA value", CSTGParamsOwner::sValueGetterTemp.value, 74L);
	s->GetAmountA(ctx);
	check_eq("CSTGSimpleAMSMixer::GetAmountA value", CSTGParamsOwner::sValueGetterTemp.value, 1707485064L);
	check_eq("CSTGSimpleAMSMixer::GetAmountA displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1707485064L);
	s->GetSourceB(ctx);
	check_eq("CSTGSimpleAMSMixer::GetSourceB value", CSTGParamsOwner::sValueGetterTemp.value, -23L);
	s->GetAmountB(ctx);
	check_eq("CSTGSimpleAMSMixer::GetAmountB value", CSTGParamsOwner::sValueGetterTemp.value, -515726588L);
	check_eq("CSTGSimpleAMSMixer::GetAmountB displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -515726588L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
