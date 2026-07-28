// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_polysix_mg_valuegetters.cpp  -  KAT for CSTGPolysixMG's
 * Get* family -- all 18 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_polysix_mg_valuegetters.cpp.
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
#include "oa_stg_polysix_mg.h"

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
	printf("CSTGPolysixMG value-getter family known-answer test (18 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGPolysixMG *s = (CSTGPolysixMG *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetKeySync(ctx);
	check_eq("CSTGPolysixMG::GetKeySync value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetMIDITempoSync(ctx);
	check_eq("CSTGPolysixMG::GetMIDITempoSync value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetMIDITempoSyncBaseNote(ctx);
	check_eq("CSTGPolysixMG::GetMIDITempoSyncBaseNote value", CSTGParamsOwner::sValueGetterTemp.value, 15L);
	s->GetFrequency(ctx);
	check_eq("CSTGPolysixMG::GetFrequency value", CSTGParamsOwner::sValueGetterTemp.value, 2078031262L);
	check_eq("CSTGPolysixMG::GetFrequency displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2078031262L);
	s->GetFrequencyAMSSource(ctx);
	check_eq("CSTGPolysixMG::GetFrequencyAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -55L);
	s->GetFrequencyAMSIntensity(ctx);
	check_eq("CSTGPolysixMG::GetFrequencyAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 713813069L);
	check_eq("CSTGPolysixMG::GetFrequencyAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 713813069L);
	s->GetFrequencyAMSIntModSource(ctx);
	check_eq("CSTGPolysixMG::GetFrequencyAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -1L);
	s->GetFrequencyAMSIntModIntensity(ctx);
	check_eq("CSTGPolysixMG::GetFrequencyAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1623270019L);
	check_eq("CSTGPolysixMG::GetFrequencyAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1623270019L);
	s->GetMIDITempoSyncTimes(ctx);
	check_eq("CSTGPolysixMG::GetMIDITempoSyncTimes value", CSTGParamsOwner::sValueGetterTemp.value, 26L);
	s->GetMIDITempoSyncTimesAMSSource(ctx);
	check_eq("CSTGPolysixMG::GetMIDITempoSyncTimesAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 88L);
	s->GetMIDITempoSyncTimesAMSIntensity(ctx);
	check_eq("CSTGPolysixMG::GetMIDITempoSyncTimesAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -71L);
	s->GetMIDITempoSyncTimesAMSIntModSource(ctx);
	check_eq("CSTGPolysixMG::GetMIDITempoSyncTimesAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -106L);
	s->GetMIDITempoSyncTimesAMSIntModIntensity(ctx);
	check_eq("CSTGPolysixMG::GetMIDITempoSyncTimesAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -9L);
	s->GetDelay(ctx);
	check_eq("CSTGPolysixMG::GetDelay value", CSTGParamsOwner::sValueGetterTemp.value, 309580853L);
	check_eq("CSTGPolysixMG::GetDelay displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 309580853L);
	s->GetDelayAMSSource(ctx);
	check_eq("CSTGPolysixMG::GetDelayAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -87L);
	s->GetDelayAMSIntensity(ctx);
	check_eq("CSTGPolysixMG::GetDelayAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1896918863L);
	check_eq("CSTGPolysixMG::GetDelayAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1896918863L);
	s->GetDelayAMSIntModSource(ctx);
	check_eq("CSTGPolysixMG::GetDelayAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, 72L);
	s->GetDelayAMSIntModIntensity(ctx);
	check_eq("CSTGPolysixMG::GetDelayAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 174836781L);
	check_eq("CSTGPolysixMG::GetDelayAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 174836781L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
