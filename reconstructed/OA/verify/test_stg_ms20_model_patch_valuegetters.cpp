// SPDX-License-Identifier: GPL-2.0
#include <cstdio>
#include <cstring>
#include "oa_stg_ms20_model_patch.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld\n", label, got); return; }
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x800
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGMS20ModelPatch value-getter family known-answer test (19 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGMS20ModelPatch *s = (CSTGMS20ModelPatch *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetMGKeySync(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGKeySync value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetMGFrequency(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGFrequency value", CSTGParamsOwner::sValueGetterTemp.value, -1071545629L);
	check_eq("CSTGMS20ModelPatch::GetMGFrequency displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1071545629L);
	s->GetMGFrequencyAMSSource(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGFrequencyAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 87L);
	s->GetMGFrequencyAMSIntensity(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGFrequencyAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1016987231L);
	check_eq("CSTGMS20ModelPatch::GetMGFrequencyAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1016987231L);
	s->GetMGFrequencyAMSIntModSource(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGFrequencyAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, -10L);
	s->GetMGFrequencyAMSIntModIntensity(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGFrequencyAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1206289701L);
	check_eq("CSTGMS20ModelPatch::GetMGFrequencyAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1206289701L);
	s->GetMGMIDITempoSync(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGMIDITempoSync value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetMGMIDITempoSyncBaseNote(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGMIDITempoSyncBaseNote value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetMGMIDITempoSyncTimes(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGMIDITempoSyncTimes value", CSTGParamsOwner::sValueGetterTemp.value, 149L);
	s->GetMGMIDITempoSyncTimesAMSSource(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGMIDITempoSyncTimesAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -45L);
	s->GetMGMIDITempoSyncTimesAMSIntensity(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGMIDITempoSyncTimesAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 52L);
	s->GetMGMIDITempoSyncTimesAMSIntModSource(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGMIDITempoSyncTimesAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, 17L);
	s->GetMGMIDITempoSyncTimesAMSIntModIntensity(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGMIDITempoSyncTimesAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 114L);
	s->GetMGWaveform(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGWaveform value", CSTGParamsOwner::sValueGetterTemp.value, 747499087L);
	check_eq("CSTGMS20ModelPatch::GetMGWaveform displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 747499087L);
	s->GetMGWaveformAMSSource(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGWaveformAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -61L);
	s->GetMGWaveformAMSIntensity(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGWaveformAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1475777845L);
	check_eq("CSTGMS20ModelPatch::GetMGWaveformAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1475777845L);
	s->GetMGWaveformAMSIntModSource(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGWaveformAMSIntModSource value", CSTGParamsOwner::sValueGetterTemp.value, 98L);
	s->GetMGWaveformAMSIntModIntensity(ctx);
	check_eq("CSTGMS20ModelPatch::GetMGWaveformAMSIntModIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 612755015L);
	check_eq("CSTGMS20ModelPatch::GetMGWaveformAMSIntModIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 612755015L);
	s->GetVoiceAllocatorEG(ctx);
	check_eq("CSTGMS20ModelPatch::GetVoiceAllocatorEG value", CSTGParamsOwner::sValueGetterTemp.value, 165L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
