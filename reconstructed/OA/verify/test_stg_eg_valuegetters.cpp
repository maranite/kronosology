// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_eg_valuegetters.cpp  -  KAT for CSTGEG's Get* family -- all
 * 10 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_eg_valuegetters.cpp.
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
#include "oa_stg_eg.h"

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
	printf("CSTGEG value-getter family known-answer test (10 methods)\n");
	printf("============================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGEG *s = (CSTGEG *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetAMS1LevelModSource(ctx);
	check_eq("CSTGEG::GetAMS1LevelModSource value", CSTGParamsOwner::sValueGetterTemp.value, -41L);
	s->GetAMS1LevelModIntensity(ctx);
	check_eq("CSTGEG::GetAMS1LevelModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1724328073L);
	check_eq("CSTGEG::GetAMS1LevelModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1724328073L);
	s->GetAMS2LevelModSource(ctx);
	check_eq("CSTGEG::GetAMS2LevelModSource value", CSTGParamsOwner::sValueGetterTemp.value, 118L);
	s->GetAMS2LevelModIntensity(ctx);
	check_eq("CSTGEG::GetAMS2LevelModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -633627395L);
	check_eq("CSTGEG::GetAMS2LevelModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -633627395L);
	s->GetAMS1TimeModSource(ctx);
	check_eq("CSTGEG::GetAMS1TimeModSource value", CSTGParamsOwner::sValueGetterTemp.value, 42L);
	s->GetAMS1TimeModIntensity(ctx);
	check_eq("CSTGEG::GetAMS1TimeModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1488525947L);
	check_eq("CSTGEG::GetAMS1TimeModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1488525947L);
	s->GetAMS2TimeModSource(ctx);
	check_eq("CSTGEG::GetAMS2TimeModSource value", CSTGParamsOwner::sValueGetterTemp.value, -55L);
	s->GetAMS2TimeModIntensity(ctx);
	check_eq("CSTGEG::GetAMS2TimeModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1219037803L);
	check_eq("CSTGEG::GetAMS2TimeModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1219037803L);
	s->GetAMS3TimeModSource(ctx);
	check_eq("CSTGEG::GetAMS3TimeModSource value", CSTGParamsOwner::sValueGetterTemp.value, 104L);
	s->GetAMS3TimeModIntensity(ctx);
	check_eq("CSTGEG::GetAMS3TimeModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 949615195L);
	check_eq("CSTGEG::GetAMS3TimeModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 949615195L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
