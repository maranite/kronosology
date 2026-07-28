// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_wave_sequence_valuegetters.cpp  -  KAT for CSTGWaveSequence's
 * Getter* family -- all 34 real weak-symbol candidates, see
 * ../src/engine/stg_wave_sequence_valuegetters.cpp.
 *
 * Expected values computed by a SEPARATE Python evaluator over the same
 * parsed offset/width/signed/ctx-index facts the source file's own
 * disassembly-derived translation used -- not by re-using the .cpp
 * file's C output strings -- against the same deterministic non-trivial
 * byte pattern as the rest of the STG value-getter family's KATs:
 * buf[i] = i times 0x9f plus 0x37, all mod 0x100. ctx.index fixed at 3,
 * matching the family's own established convention.
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

#define BUFSZ 0x400
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGWaveSequence value-getter family known-answer test (34 methods)\n");
	printf("=====================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);

	CSTGWaveSequence *s = (CSTGWaveSequence *)g_buf;
	CSTGWaveSeqDataMessageContext &ctx = *(CSTGWaveSeqDataMessageContext *)g_ctxbuf;
	ctx.index = 3;

	s->GetterRunSequence(ctx);
	check_eq("CSTGWaveSequence::GetterRunSequence value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetterNoteOnAdvance(ctx);
	check_eq("CSTGWaveSequence::GetterNoteOnAdvance value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetterTimeTempoMode(ctx);
	check_eq("CSTGWaveSequence::GetterTimeTempoMode value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetterSwingResolution(ctx);
	check_eq("CSTGWaveSequence::GetterSwingResolution value", CSTGParamsOwner::sValueGetterTemp.value, 82L);
	s->GetterStartStep(ctx);
	check_eq("CSTGWaveSequence::GetterStartStep value", CSTGParamsOwner::sValueGetterTemp.value, 241L);
	s->GetterEndStep(ctx);
	check_eq("CSTGWaveSequence::GetterEndStep value", CSTGParamsOwner::sValueGetterTemp.value, 144L);
	s->GetterSoloStep(ctx);
	check_eq("CSTGWaveSequence::GetterSoloStep value", CSTGParamsOwner::sValueGetterTemp.value, 47L);
	s->GetterStartStepAMSSource(ctx);
	check_eq("CSTGWaveSequence::GetterStartStepAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 206L);
	s->GetterStartStepAMSIntensity(ctx);
	check_eq("CSTGWaveSequence::GetterStartStepAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 109L);
	s->GetterDurationAMSSource(ctx);
	check_eq("CSTGWaveSequence::GetterDurationAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 12L);
	s->GetterDurationAMSIntensity(ctx);
	check_eq("CSTGWaveSequence::GetterDurationAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 19115L);
	s->GetterPositionAMSSource(ctx);
	check_eq("CSTGWaveSequence::GetterPositionAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 233L);
	s->GetterPositionAMSIntensity(ctx);
	check_eq("CSTGWaveSequence::GetterPositionAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -120L);
	s->GetterLoopStart(ctx);
	check_eq("CSTGWaveSequence::GetterLoopStart value", CSTGParamsOwner::sValueGetterTemp.value, 39L);
	s->GetterLoopEnd(ctx);
	check_eq("CSTGWaveSequence::GetterLoopEnd value", CSTGParamsOwner::sValueGetterTemp.value, 198L);
	s->GetterLoopRepeat(ctx);
	check_eq("CSTGWaveSequence::GetterLoopRepeat value", CSTGParamsOwner::sValueGetterTemp.value, 101L);
	s->GetterLoopDirection(ctx);
	check_eq("CSTGWaveSequence::GetterLoopDirection value", CSTGParamsOwner::sValueGetterTemp.value, 4L);

	s->GetterStepType(ctx);
	check_eq("CSTGWaveSequence::GetterStepType value", CSTGParamsOwner::sValueGetterTemp.value, 25L);
	s->GetterMultisampleSelect(ctx);
	check_eq("CSTGWaveSequence::GetterMultisampleSelect value", CSTGParamsOwner::sValueGetterTemp.value, 65119L);
	s->GetterLevel(ctx);
	check_eq("CSTGWaveSequence::GetterLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1421153911L);
	check_eq("CSTGWaveSequence::GetterLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1421153911L);
	s->GetterTune(ctx);
	check_eq("CSTGWaveSequence::GetterTune value", CSTGParamsOwner::sValueGetterTemp.value, -802057485L);
	check_eq("CSTGWaveSequence::GetterTune displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -802057485L);
	s->GetterTranspose(ctx);
	check_eq("CSTGWaveSequence::GetterTranspose value", CSTGParamsOwner::sValueGetterTemp.value, -72L);
	s->GetterReverse(ctx);
	check_eq("CSTGWaveSequence::GetterReverse value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetterStartOffset(ctx);
	check_eq("CSTGWaveSequence::GetterStartOffset value", CSTGParamsOwner::sValueGetterTemp.value, 87L);
	s->GetterAMS1Output(ctx);
	check_eq("CSTGWaveSequence::GetterAMS1Output value", CSTGParamsOwner::sValueGetterTemp.value, 1286409839L);
	check_eq("CSTGWaveSequence::GetterAMS1Output displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1286409839L);
	s->GetterAMS2Output(ctx);
	check_eq("CSTGWaveSequence::GetterAMS2Output value", CSTGParamsOwner::sValueGetterTemp.value, -936801557L);
	check_eq("CSTGWaveSequence::GetterAMS2Output displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -936801557L);
	s->GetterDuration(ctx);
	check_eq("CSTGWaveSequence::GetterDuration value", CSTGParamsOwner::sValueGetterTemp.value, 15517L);
	s->GetterTempoBaseNote(ctx);
	check_eq("CSTGWaveSequence::GetterTempoBaseNote value", CSTGParamsOwner::sValueGetterTemp.value, -10L);
	s->GetterTempoMultiplier(ctx);
	check_eq("CSTGWaveSequence::GetterTempoMultiplier value", CSTGParamsOwner::sValueGetterTemp.value, 149L);
	s->GetterCrossfadeTime(ctx);
	check_eq("CSTGWaveSequence::GetterCrossfadeTime value", CSTGParamsOwner::sValueGetterTemp.value, 31451L);
	s->GetterFadeOutShape(ctx);
	check_eq("CSTGWaveSequence::GetterFadeOutShape value", CSTGParamsOwner::sValueGetterTemp.value, 1151665767L);
	check_eq("CSTGWaveSequence::GetterFadeOutShape displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1151665767L);
	s->GetterFadeInShape(ctx);
	check_eq("CSTGWaveSequence::GetterFadeInShape value", CSTGParamsOwner::sValueGetterTemp.value, -1071545629L);
	check_eq("CSTGWaveSequence::GetterFadeInShape displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1071545629L);

	s->GetterBankSelect(ctx);
	{
		unsigned char *temp = (unsigned char *)&CSTGParamsOwner::sValueGetterTemp;
		check_eq("CSTGWaveSequence::GetterBankSelect uuid[0]", *(int *)(temp + 0x0), 1690642055L);
		check_eq("CSTGWaveSequence::GetterBankSelect uuid[1]", *(int *)(temp + 0x4), -532569597L);
		check_eq("CSTGWaveSequence::GetterBankSelect uuid[2]", *(int *)(temp + 0x8), 1555897983L);
		check_eq("CSTGWaveSequence::GetterBankSelect uuid[3]", *(int *)(temp + 0xc), -667313413L);
	}
	s->GetterBankSelectUUID(ctx);
	{
		unsigned char *temp = (unsigned char *)&CSTGParamsOwner::sValueGetterTemp;
		check_eq("CSTGWaveSequence::GetterBankSelectUUID uuid[0]", *(int *)(temp + 0x0), 1690642055L);
		check_eq("CSTGWaveSequence::GetterBankSelectUUID uuid[1]", *(int *)(temp + 0x4), -532569597L);
		check_eq("CSTGWaveSequence::GetterBankSelectUUID uuid[2]", *(int *)(temp + 0x8), 1555897983L);
		check_eq("CSTGWaveSequence::GetterBankSelectUUID uuid[3]", *(int *)(temp + 0xc), -667313413L);
	}

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
