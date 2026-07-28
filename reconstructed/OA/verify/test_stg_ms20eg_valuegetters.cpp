// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_ms20eg_valuegetters.cpp  -  KAT for CSTGMS20EG's Get*
 * family -- all 20 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_ms20eg_valuegetters.cpp.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own
 * decoder used -- not by re-using the .cpp file's C output strings --
 * against the same deterministic non-trivial byte pattern as the rest
 * of the STG value-getter family's KATs: buf[i] = i times 0x9f plus
 * 0x37, all mod 0x100. This class has no ctx-dynamic-index methods, so
 * the ctx buffer's own contents are never actually read.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_ms20eg.h"

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
	printf("CSTGMS20EG value-getter family known-answer test (20 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGMS20EG *s = (CSTGMS20EG *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetEG2HoldTime(ctx);
	check_eq("CSTGMS20EG::GetEG2HoldTime value", CSTGParamsOwner::sValueGetterTemp.value, 1353781875L);
	check_eq("CSTGMS20EG::GetEG2HoldTime displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1353781875L);
	s->GetEG2HoldTimeAMSSource(ctx);
	check_eq("CSTGMS20EG::GetEG2HoldTimeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -25L);
	s->GetEG2HoldTimeAMSIntensity(ctx);
	check_eq("CSTGMS20EG::GetEG2HoldTimeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -869429521L);
	check_eq("CSTGMS20EG::GetEG2HoldTimeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -869429521L);
	s->GetEG2HoldTimeAMSIntensityAMSSource(ctx);
	check_eq("CSTGMS20EG::GetEG2HoldTimeAMSIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -122L);
	s->GetEG2HoldTimeAMSIntensityAMSIntensity(ctx);
	check_eq("CSTGMS20EG::GetEG2HoldTimeAMSIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1219037803L);
	check_eq("CSTGMS20EG::GetEG2HoldTimeAMSIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1219037803L);
	s->GetEG1DelayTime(ctx);
	check_eq("CSTGMS20EG::GetEG1DelayTime value", CSTGParamsOwner::sValueGetterTemp.value, 40092709L);
	check_eq("CSTGMS20EG::GetEG1DelayTime displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 40092709L);
	s->GetEG1DelayTimeAMSSource(ctx);
	check_eq("CSTGMS20EG::GetEG1DelayTimeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -103L);
	s->GetEG1DelayTimeAMSIntensity(ctx);
	check_eq("CSTGMS20EG::GetEG1DelayTimeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 2128560289L);
	check_eq("CSTGMS20EG::GetEG1DelayTimeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2128560289L);
	s->GetEG1DelayTimeAMSIntensityAMSSource(ctx);
	check_eq("CSTGMS20EG::GetEG1DelayTimeAMSIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 56L);
	s->GetEG1DelayTimeAMSIntensityAMSIntensity(ctx);
	check_eq("CSTGMS20EG::GetEG1DelayTimeAMSIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -94651363L);
	check_eq("CSTGMS20EG::GetEG1DelayTimeAMSIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -94651363L);
	s->GetEG1AttackTime(ctx);
	check_eq("CSTGMS20EG::GetEG1AttackTime value", CSTGParamsOwner::sValueGetterTemp.value, -1273661737L);
	check_eq("CSTGMS20EG::GetEG1AttackTime displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1273661737L);
	s->GetEG1AttackTimeAMSSource(ctx);
	check_eq("CSTGMS20EG::GetEG1AttackTimeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 75L);
	s->GetEG1AttackTimeAMSIntensity(ctx);
	check_eq("CSTGMS20EG::GetEG1AttackTimeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 814871123L);
	check_eq("CSTGMS20EG::GetEG1AttackTimeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 814871123L);
	s->GetEG1AttackTimeAMSIntensityAMSSource(ctx);
	check_eq("CSTGMS20EG::GetEG1AttackTimeAMSIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -22L);
	s->GetEG1AttackTimeAMSIntensityAMSIntensity(ctx);
	check_eq("CSTGMS20EG::GetEG1AttackTimeAMSIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1408405809L);
	check_eq("CSTGMS20EG::GetEG1AttackTimeAMSIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1408405809L);
	s->GetEG1ReleaseTime(ctx);
	check_eq("CSTGMS20EG::GetEG1ReleaseTime value", CSTGParamsOwner::sValueGetterTemp.value, 1724328073L);
	check_eq("CSTGMS20EG::GetEG1ReleaseTime displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1724328073L);
	s->GetEG1ReleaseTimeAMSSource(ctx);
	check_eq("CSTGMS20EG::GetEG1ReleaseTimeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -3L);
	s->GetEG1ReleaseTimeAMSIntensity(ctx);
	check_eq("CSTGMS20EG::GetEG1ReleaseTimeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -498883579L);
	check_eq("CSTGMS20EG::GetEG1ReleaseTimeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -498883579L);
	s->GetEG1ReleaseTimeAMSIntensityAMSSource(ctx);
	check_eq("CSTGMS20EG::GetEG1ReleaseTimeAMSIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -100L);
	s->GetEG1ReleaseTimeAMSIntensityAMSIntensity(ctx);
	check_eq("CSTGMS20EG::GetEG1ReleaseTimeAMSIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1589584001L);
	check_eq("CSTGMS20EG::GetEG1ReleaseTimeAMSIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1589584001L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
