// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_eg_base_valuegetters.cpp  -  KAT for CSTGEGBase's Get*
 * family -- all 5 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_eg_base_valuegetters.cpp.
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
#include "oa_stg_eg_base.h"

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
	printf("CSTGEGBase value-getter family known-answer test (5 methods)\n");
	printf("============================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGEGBase *s = (CSTGEGBase *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetLevel(ctx);
	check_eq("CSTGEGBase::GetLevel value", CSTGParamsOwner::sValueGetterTemp.value, 2027502235L);
	check_eq("CSTGEGBase::GetLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2027502235L);
	s->GetTime(ctx);
	check_eq("CSTGEGBase::GetTime value", CSTGParamsOwner::sValueGetterTemp.value, 112L);
	s->GetCurve(ctx);
	check_eq("CSTGEGBase::GetCurve value", CSTGParamsOwner::sValueGetterTemp.value, 136L);
	s->GetAMSResetSource(ctx);
	check_eq("CSTGEGBase::GetAMSResetSource value", CSTGParamsOwner::sValueGetterTemp.value, -117L);
	s->GetAMSResetThreshold(ctx);
	check_eq("CSTGEGBase::GetAMSResetThreshold value", CSTGParamsOwner::sValueGetterTemp.value, -330453489L);
	check_eq("CSTGEGBase::GetAMSResetThreshold displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -330453489L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
