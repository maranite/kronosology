// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_combi_valuegetters.cpp  -  KAT for CSTGCombi's Get* family --
 * all 4 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_combi_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed facts the source file's own
 * disassembly-derived translation used -- not by re-using the .cpp
 * file's C output strings -- against the same deterministic non-trivial
 * byte pattern as the rest of the STG value-getter family's KATs:
 * buf[i] = i times 0x9f plus 0x37, all mod 0x100. This class has no
 * ctx-dynamic-index methods, so the ctx buffer's own contents are never
 * actually read.
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

#define BUFSZ 0x1a00
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGCombi value-getter family known-answer test (4 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);

	CSTGCombi *s = (CSTGCombi *)g_buf;
	CSTGMessageContext &ctx = *(CSTGMessageContext *)g_ctxbuf;

	s->GetValueScaleType(ctx);
	check_eq("CSTGCombi::GetValueScaleType value", CSTGParamsOwner::sValueGetterTemp.value, 52L);
	s->GetValuePitchRandomize(ctx);
	check_eq("CSTGCombi::GetValuePitchRandomize value", CSTGParamsOwner::sValueGetterTemp.value, 211L);
	s->GetValueScaleKey(ctx);
	check_eq("CSTGCombi::GetValueScaleKey value", CSTGParamsOwner::sValueGetterTemp.value, 114L);
	s->GetValueAutoLoadToneAdjust(ctx);
	check_eq("CSTGCombi::GetValueAutoLoadToneAdjust value", CSTGParamsOwner::sValueGetterTemp.value, 1L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
