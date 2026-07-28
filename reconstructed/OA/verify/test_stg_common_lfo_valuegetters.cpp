// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_common_lfo_valuegetters.cpp  -  KAT for CSTGCommonLFO's
 * Get* family -- all 17 real weak-symbol ctx-only candidates, see
 * ../src/engine/stg_common_lfo_valuegetters.cpp. This test ONLY
 * exercises the value-getter methods (via a raw cast over a
 * deterministic byte buffer) -- it does not construct a real
 * CSTGCommonLFO or call Initialize()/the ctor, matching this family's
 * established KAT convention.
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed offset/width/signed/dual facts the source file's own
 * disassembly-derived translation used -- not by re-using the .cpp
 * file's C output strings -- against the same deterministic non-trivial
 * byte pattern as the rest of the STG value-getter family's KATs:
 * buf[i] = i times 0x9f plus 0x37, all mod 0x100. This class has no
 * ctx-dynamic-index methods (confirmed via direct disassembly, despite
 * several method names implying otherwise), so the ctx buffer's own
 * contents are never actually read.
 */

#include <cstdio>
#include <cstring>
#include "oa_global.h"
#include "oa_engine_init.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;
STGLFOSubRateParams *CSTGCommonLFO::sSubRateParams;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-50s %ld\n", label, got); return; }
	printf("  FAIL  %-50s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x100
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGCommonLFO value-getter family known-answer test (17 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);

	CSTGCommonLFO *s = (CSTGCommonLFO *)g_buf;
	CSTGProgramMessageContext &ctx = *(CSTGProgramMessageContext *)g_ctxbuf;

	s->GetWaveform(ctx);
	check_eq("CSTGCommonLFO::GetWaveform value", CSTGParamsOwner::sValueGetterTemp.value, -4L);
	s->GetStartPhase(ctx);
	check_eq("CSTGCommonLFO::GetStartPhase value", CSTGParamsOwner::sValueGetterTemp.value, 47L);
	s->GetShape(ctx);
	check_eq("CSTGCommonLFO::GetShape value", CSTGParamsOwner::sValueGetterTemp.value, -1425248818L);
	check_eq("CSTGCommonLFO::GetShape displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1425248818L);
	s->GetShapeAMSSource(ctx);
	check_eq("CSTGCommonLFO::GetShapeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -58L);
	s->GetShapeAMSIntensity(ctx);
	check_eq("CSTGCommonLFO::GetShapeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 663284042L);
	check_eq("CSTGCommonLFO::GetShapeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 663284042L);
	s->GetOffset(ctx);
	check_eq("CSTGCommonLFO::GetOffset value", CSTGParamsOwner::sValueGetterTemp.value, 1117979749L);
	check_eq("CSTGCommonLFO::GetOffset displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1117979749L);
	s->GetFrequency(ctx);
	check_eq("CSTGCommonLFO::GetFrequency value", CSTGParamsOwner::sValueGetterTemp.value, 155L);
	s->GetFrequencyFine(ctx);
	check_eq("CSTGCommonLFO::GetFrequencyFine value", CSTGParamsOwner::sValueGetterTemp.value, -1105231647L);
	check_eq("CSTGCommonLFO::GetFrequencyFine displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1105231647L);
	s->GetAMSResetSource(ctx);
	check_eq("CSTGCommonLFO::GetAMSResetSource value", CSTGParamsOwner::sValueGetterTemp.value, 93L);
	s->GetFrequencyAMSSource(ctx);
	check_eq("CSTGCommonLFO::GetFrequencyAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -47L);
	s->GetFrequencyAMSIntensity(ctx);
	check_eq("CSTGCommonLFO::GetFrequencyAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 848557141L);
	check_eq("CSTGCommonLFO::GetFrequencyAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 848557141L);
	s->GetFrequencyAMSIntensityAMSSource(ctx);
	check_eq("CSTGCommonLFO::GetFrequencyAMSIntensityAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 7L);
	s->GetFrequencyAMSIntensityAMSIntensity(ctx);
	check_eq("CSTGCommonLFO::GetFrequencyAMSIntensityAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1758014091L);
	check_eq("CSTGCommonLFO::GetFrequencyAMSIntensityAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1758014091L);
	s->GetStop(ctx);
	check_eq("CSTGCommonLFO::GetStop value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetMIDITempoSync(ctx);
	check_eq("CSTGCommonLFO::GetMIDITempoSync value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetMIDITempoSyncBaseNote(ctx);
	check_eq("CSTGCommonLFO::GetMIDITempoSyncBaseNote value", CSTGParamsOwner::sValueGetterTemp.value, 23L);
	s->GetMIDITempoSyncTimes(ctx);
	check_eq("CSTGCommonLFO::GetMIDITempoSyncTimes value", CSTGParamsOwner::sValueGetterTemp.value, 182L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
