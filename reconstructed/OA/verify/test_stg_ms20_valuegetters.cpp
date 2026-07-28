// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_ms20_valuegetters.cpp  -  KAT for CSTGMS20's Get*() family
 * (90 methods, see ../src/engine/stg_ms20_valuegetters.cpp).
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed (offset, ctx-index*stride, width, signed, dual) facts the
 * source file's own decoder used -- not by re-using stg_ms20_valuegetters.
 * cpp's C output strings -- against the same deterministic non-trivial
 * byte pattern as the CSTGString/CSTGOrganModelPatch KATs (buf[i] =
 * (i*0x9f+0x37) & 0xff). ctx's own dynamic-index field (+0x4) is fixed at
 * 3 for every ctx-indexed getter, matching CSTGString's KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_ms20.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld\n", label, got); return; }
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x240
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGMS20 value-getter family known-answer test (90 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGMS20 *s = (CSTGMS20 *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetAnalog(ctx);
	check_eq("CSTGMS20::GetAnalog value", CSTGParamsOwner::sValueGetterTemp.value, -77808354L);
	check_eq("CSTGMS20::GetAnalog displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -77808354L);
	s->GetEspCVAdjust(ctx);
	check_eq("CSTGMS20::GetEspCVAdjust value", CSTGParamsOwner::sValueGetterTemp.value, -852586512L);
	check_eq("CSTGMS20::GetEspCVAdjust displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -852586512L);
	s->GetEspCVAdjustAMSIntensity(ctx);
	check_eq("CSTGMS20::GetEspCVAdjustAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1235880812L);
	check_eq("CSTGMS20::GetEspCVAdjustAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1235880812L);
	s->GetEspCVAdjustAMSSource(ctx);
	check_eq("CSTGMS20::GetEspCVAdjustAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -24L);
	s->GetEspHighCutFreq(ctx);
	check_eq("CSTGMS20::GetEspHighCutFreq value", CSTGParamsOwner::sValueGetterTemp.value, 915929177L);
	check_eq("CSTGMS20::GetEspHighCutFreq displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 915929177L);
	s->GetEspHighCutFreqAMSIntensity(ctx);
	check_eq("CSTGMS20::GetEspHighCutFreqAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1307347755L);
	check_eq("CSTGMS20::GetEspHighCutFreqAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1307347755L);
	s->GetEspHighCutFreqAMSSource(ctx);
	check_eq("CSTGMS20::GetEspHighCutFreqAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 81L);
	s->GetEspLowCutFreq(ctx);
	check_eq("CSTGMS20::GetEspLowCutFreq value", CSTGParamsOwner::sValueGetterTemp.value, -1627364926L);
	check_eq("CSTGMS20::GetEspLowCutFreq displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1627364926L);
	s->GetEspLowCutFreqAMSIntensity(ctx);
	check_eq("CSTGMS20::GetEspLowCutFreqAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 461167934L);
	check_eq("CSTGMS20::GetEspLowCutFreqAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 461167934L);
	s->GetEspLowCutFreqAMSSource(ctx);
	check_eq("CSTGMS20::GetEspLowCutFreqAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -70L);
	s->GetEspSignalLevel(ctx);
	check_eq("CSTGMS20::GetEspSignalLevel value", CSTGParamsOwner::sValueGetterTemp.value, 141150763L);
	check_eq("CSTGMS20::GetEspSignalLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 141150763L);
	s->GetEspSignalLevelAMSIntensity(ctx);
	check_eq("CSTGMS20::GetEspSignalLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -2065348953L);
	check_eq("CSTGMS20::GetEspSignalLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2065348953L);
	s->GetEspSignalLevelAMSSource(ctx);
	check_eq("CSTGMS20::GetEspSignalLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 35L);
	s->GetEspThreshold(ctx);
	check_eq("CSTGMS20::GetEspThreshold value", CSTGParamsOwner::sValueGetterTemp.value, 1690642055L);
	check_eq("CSTGMS20::GetEspThreshold displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1690642055L);
	s->GetEspThresholdAMSIntensity(ctx);
	check_eq("CSTGMS20::GetEspThresholdAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -532569597L);
	check_eq("CSTGMS20::GetEspThresholdAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -532569597L);
	s->GetEspThresholdAMSSource(ctx);
	check_eq("CSTGMS20::GetEspThresholdAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 127L);
	s->GetExtModASource(ctx);
	check_eq("CSTGMS20::GetExtModASource value", CSTGParamsOwner::sValueGetterTemp.value, 120L);
	s->GetExtModAtoAmp(ctx);
	check_eq("CSTGMS20::GetExtModAtoAmp value", CSTGParamsOwner::sValueGetterTemp.value, 1842229136L);
	check_eq("CSTGMS20::GetExtModAtoAmp displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1842229136L);
	s->GetExtModAtoHpfFc(ctx);
	check_eq("CSTGMS20::GetExtModAtoHpfFc value", CSTGParamsOwner::sValueGetterTemp.value, 2111717280L);
	check_eq("CSTGMS20::GetExtModAtoHpfFc displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2111717280L);
	s->GetExtModAtoLpfFc(ctx);
	check_eq("CSTGMS20::GetExtModAtoLpfFc value", CSTGParamsOwner::sValueGetterTemp.value, -111494372L);
	check_eq("CSTGMS20::GetExtModAtoLpfFc displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -111494372L);
	s->GetExtModAtoVco1Pw(ctx);
	check_eq("CSTGMS20::GetExtModAtoVco1Pw value", CSTGParamsOwner::sValueGetterTemp.value, 1976973208L);
	check_eq("CSTGMS20::GetExtModAtoVco1Pw displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1976973208L);
	s->GetExtModAtoVco2Pitch(ctx);
	check_eq("CSTGMS20::GetExtModAtoVco2Pitch value", CSTGParamsOwner::sValueGetterTemp.value, -246238444L);
	check_eq("CSTGMS20::GetExtModAtoVco2Pitch displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -246238444L);
	s->GetExtModBSource(ctx);
	check_eq("CSTGMS20::GetExtModBSource value", CSTGParamsOwner::sValueGetterTemp.value, 23L);
	s->GetExtModBtoHpfFc(ctx);
	check_eq("CSTGMS20::GetExtModBtoHpfFc value", CSTGParamsOwner::sValueGetterTemp.value, -380982516L);
	check_eq("CSTGMS20::GetExtModBtoHpfFc displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -380982516L);
	s->GetExtModBtoHpfTExt(ctx);
	check_eq("CSTGMS20::GetExtModBtoHpfTExt value", CSTGParamsOwner::sValueGetterTemp.value, 1572740992L);
	check_eq("CSTGMS20::GetExtModBtoHpfTExt displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1572740992L);
	s->GetExtModBtoLpfFc(ctx);
	check_eq("CSTGMS20::GetExtModBtoLpfFc value", CSTGParamsOwner::sValueGetterTemp.value, 1707485064L);
	check_eq("CSTGMS20::GetExtModBtoLpfFc displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1707485064L);
	s->GetExtModBtoLpfTExt(ctx);
	check_eq("CSTGMS20::GetExtModBtoLpfTExt value", CSTGParamsOwner::sValueGetterTemp.value, -650470404L);
	check_eq("CSTGMS20::GetExtModBtoLpfTExt displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -650470404L);
	s->GetExtModBtoVcoTExt(ctx);
	check_eq("CSTGMS20::GetExtModBtoVcoTExt value", CSTGParamsOwner::sValueGetterTemp.value, -515726588L);
	check_eq("CSTGMS20::GetExtModBtoVcoTExt displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -515726588L);
	s->GetFineTune(ctx);
	check_eq("CSTGMS20::GetFineTune value", CSTGParamsOwner::sValueGetterTemp.value, 1488525947L);
	check_eq("CSTGMS20::GetFineTune displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1488525947L);
	s->GetFineTuneAMSIntAMSIntensity(ctx);
	check_eq("CSTGMS20::GetFineTuneAMSIntAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1353781875L);
	check_eq("CSTGMS20::GetFineTuneAMSIntAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1353781875L);
	s->GetFineTuneAMSIntAMSSource(ctx);
	check_eq("CSTGMS20::GetFineTuneAMSIntAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -114L);
	s->GetFineTuneAMSIntensity(ctx);
	check_eq("CSTGMS20::GetFineTuneAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -734685449L);
	check_eq("CSTGMS20::GetFineTuneAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -734685449L);
	s->GetFineTuneAMSSource(ctx);
	check_eq("CSTGMS20::GetFineTuneAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -17L);
	s->GetFreqModEg1Ext(ctx);
	check_eq("CSTGMS20::GetFreqModEg1Ext value", CSTGParamsOwner::sValueGetterTemp.value, -2031662935L);
	check_eq("CSTGMS20::GetFreqModEg1Ext displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2031662935L);
	s->GetFreqModMgTExt(ctx);
	check_eq("CSTGMS20::GetFreqModMgTExt value", CSTGParamsOwner::sValueGetterTemp.value, 174836781L);
	check_eq("CSTGMS20::GetFreqModMgTExt displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 174836781L);
	s->GetHpfCutoff(ctx);
	check_eq("CSTGMS20::GetHpfCutoff value", CSTGParamsOwner::sValueGetterTemp.value, -1408405809L);
	check_eq("CSTGMS20::GetHpfCutoff displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1408405809L);
	s->GetHpfEg2Ext(ctx);
	check_eq("CSTGMS20::GetHpfEg2Ext value", CSTGParamsOwner::sValueGetterTemp.value, 545382979L);
	check_eq("CSTGMS20::GetHpfEg2Ext displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 545382979L);
	s->GetHpfMgText(ctx);
	check_eq("CSTGMS20::GetHpfMgText value", CSTGParamsOwner::sValueGetterTemp.value, -1543149881L);
	check_eq("CSTGMS20::GetHpfMgText displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1543149881L);
	s->GetHpfPeak(ctx);
	check_eq("CSTGMS20::GetHpfPeak value", CSTGParamsOwner::sValueGetterTemp.value, 680127051L);
	check_eq("CSTGMS20::GetHpfPeak displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 680127051L);
	s->GetInputJack(ctx);
	check_eq("CSTGMS20::GetInputJack value", CSTGParamsOwner::sValueGetterTemp.value, -99L);
	s->GetLpfCutoff(ctx);
	check_eq("CSTGMS20::GetLpfCutoff value", CSTGParamsOwner::sValueGetterTemp.value, -1661116737L);
	check_eq("CSTGMS20::GetLpfCutoff displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1661116737L);
	s->GetLpfEg2Ext(ctx);
	check_eq("CSTGMS20::GetLpfEg2Ext value", CSTGParamsOwner::sValueGetterTemp.value, 275894835L);
	check_eq("CSTGMS20::GetLpfEg2Ext displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 275894835L);
	s->GetLpfMgText(ctx);
	check_eq("CSTGMS20::GetLpfMgText value", CSTGParamsOwner::sValueGetterTemp.value, -1795860809L);
	check_eq("CSTGMS20::GetLpfMgText displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1795860809L);
	s->GetLpfPeak(ctx);
	check_eq("CSTGMS20::GetLpfPeak value", CSTGParamsOwner::sValueGetterTemp.value, 410638907L);
	check_eq("CSTGMS20::GetLpfPeak displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 410638907L);
	s->GetMixer1LevelA(ctx);
	check_eq("CSTGMS20::GetMixer1LevelA value", CSTGParamsOwner::sValueGetterTemp.value, -1391562800L);
	check_eq("CSTGMS20::GetMixer1LevelA displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1391562800L);
	s->GetMixer1LevelB(ctx);
	check_eq("CSTGMS20::GetMixer1LevelB value", CSTGParamsOwner::sValueGetterTemp.value, 696970060L);
	check_eq("CSTGMS20::GetMixer1LevelB displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 696970060L);
	s->GetMixer2LevelA(ctx);
	check_eq("CSTGMS20::GetMixer2LevelA value", CSTGParamsOwner::sValueGetterTemp.value, -1526306872L);
	check_eq("CSTGMS20::GetMixer2LevelA displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1526306872L);
	s->GetMixer2LevelB(ctx);
	check_eq("CSTGMS20::GetMixer2LevelB value", CSTGParamsOwner::sValueGetterTemp.value, 562225988L);
	check_eq("CSTGMS20::GetMixer2LevelB displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 562225988L);
	s->GetMixerAMSIntAMSIntensity(ctx);
	check_eq("CSTGMS20::GetMixerAMSIntAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1320095857L);
	check_eq("CSTGMS20::GetMixerAMSIntAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1320095857L);
	s->GetMixerAMSIntAMSSource(ctx);
	check_eq("CSTGMS20::GetMixerAMSIntAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -116L);
	s->GetMixerAMSIntensity(ctx);
	check_eq("CSTGMS20::GetMixerAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -768371467L);
	check_eq("CSTGMS20::GetMixerAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -768371467L);
	s->GetMixerAMSSource(ctx);
	check_eq("CSTGMS20::GetMixerAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -19L);
	s->GetMomentarySwAMSIntensity(ctx);
	check_eq("CSTGMS20::GetMomentarySwAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1829546827L);
	check_eq("CSTGMS20::GetMomentarySwAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1829546827L);
	s->GetMomentarySwAMSSource(ctx);
	check_eq("CSTGMS20::GetMomentarySwAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 49L);
	s->GetPortamento(ctx);
	check_eq("CSTGMS20::GetPortamento value", CSTGParamsOwner::sValueGetterTemp.value, -2132720989L);
	check_eq("CSTGMS20::GetPortamento displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2132720989L);
	s->GetStandardAMSIntAMSIntensity(ctx);
	check_eq("CSTGMS20::GetStandardAMSIntAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1522211965L);
	check_eq("CSTGMS20::GetStandardAMSIntAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1522211965L);
	s->GetStandardAMSIntAMSSource(ctx);
	check_eq("CSTGMS20::GetStandardAMSIntAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -104L);
	s->GetStandardAMSIntensity(ctx);
	check_eq("CSTGMS20::GetStandardAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -566255615L);
	check_eq("CSTGMS20::GetStandardAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -566255615L);
	s->GetStandardAMSSource(ctx);
	check_eq("CSTGMS20::GetStandardAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -7L);
	s->GetTranspose(ctx);
	check_eq("CSTGMS20::GetTranspose value", CSTGParamsOwner::sValueGetterTemp.value, -94651363L);
	check_eq("CSTGMS20::GetTranspose displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -94651363L);
	s->GetTransposeAMSIntAMSIntensity(ctx);
	check_eq("CSTGMS20::GetTransposeAMSIntAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -229395435L);
	check_eq("CSTGMS20::GetTransposeAMSIntAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -229395435L);
	s->GetTransposeAMSIntAMSSource(ctx);
	check_eq("CSTGMS20::GetTransposeAMSIntAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 48L);
	s->GetTransposeAMSIntensity(ctx);
	check_eq("CSTGMS20::GetTransposeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1993816217L);
	check_eq("CSTGMS20::GetTransposeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1993816217L);
	s->GetTransposeAMSSource(ctx);
	check_eq("CSTGMS20::GetTransposeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -111L);
	s->GetTriggerOn(ctx);
	check_eq("CSTGMS20::GetTriggerOn value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetVco1Level(ctx);
	check_eq("CSTGMS20::GetVco1Level value", CSTGParamsOwner::sValueGetterTemp.value, 40092709L);
	check_eq("CSTGMS20::GetVco1Level displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 40092709L);
	s->GetVco1PulseWidth(ctx);
	check_eq("CSTGMS20::GetVco1PulseWidth value", CSTGParamsOwner::sValueGetterTemp.value, -1812703818L);
	check_eq("CSTGMS20::GetVco1PulseWidth displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1812703818L);
	s->GetVco1Scale(ctx);
	check_eq("CSTGMS20::GetVco1Scale value", CSTGParamsOwner::sValueGetterTemp.value, 50L);
	s->GetVco1ScaleAMSIntensity(ctx);
	check_eq("CSTGMS20::GetVco1ScaleAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -47L);
	s->GetVco1ScaleAMSSource(ctx);
	check_eq("CSTGMS20::GetVco1ScaleAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 112L);
	s->GetVco1Waveform(ctx);
	check_eq("CSTGMS20::GetVco1Waveform value", CSTGParamsOwner::sValueGetterTemp.value, -39L);
	s->GetVco1WaveformAMSIntensity(ctx);
	check_eq("CSTGMS20::GetVco1WaveformAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 120L);
	s->GetVco1WaveformAMSSource(ctx);
	check_eq("CSTGMS20::GetVco1WaveformAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 23L);
	s->GetVco2Level(ctx);
	check_eq("CSTGMS20::GetVco2Level value", CSTGParamsOwner::sValueGetterTemp.value, 2128560289L);
	check_eq("CSTGMS20::GetVco2Level displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2128560289L);
	s->GetVco2Pitch(ctx);
	check_eq("CSTGMS20::GetVco2Pitch value", CSTGParamsOwner::sValueGetterTemp.value, -919958548L);
	check_eq("CSTGMS20::GetVco2Pitch displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -919958548L);
	s->GetVco2PitchAMSIntAMSIntensity(ctx);
	check_eq("CSTGMS20::GetVco2PitchAMSIntAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1054702620L);
	check_eq("CSTGMS20::GetVco2PitchAMSIntAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1054702620L);
	s->GetVco2PitchAMSIntAMSSource(ctx);
	check_eq("CSTGMS20::GetVco2PitchAMSIntAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -1L);
	s->GetVco2PitchAMSIntensity(ctx);
	check_eq("CSTGMS20::GetVco2PitchAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1168508776L);
	check_eq("CSTGMS20::GetVco2PitchAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1168508776L);
	s->GetVco2PitchAMSSource(ctx);
	check_eq("CSTGMS20::GetVco2PitchAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 96L);
	s->GetVco2Scale(ctx);
	check_eq("CSTGMS20::GetVco2Scale value", CSTGParamsOwner::sValueGetterTemp.value, -98L);
	s->GetVco2ScaleAMSIntensity(ctx);
	check_eq("CSTGMS20::GetVco2ScaleAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 61L);
	s->GetVco2ScaleAMSSource(ctx);
	check_eq("CSTGMS20::GetVco2ScaleAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -36L);
	s->GetVco2Waveform(ctx);
	check_eq("CSTGMS20::GetVco2Waveform value", CSTGParamsOwner::sValueGetterTemp.value, 15L);
	s->GetVco2WaveformAMSIntensity(ctx);
	check_eq("CSTGMS20::GetVco2WaveformAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -82L);
	s->GetVco2WaveformAMSSource(ctx);
	check_eq("CSTGMS20::GetVco2WaveformAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 77L);
	s->GetVolume(ctx);
	check_eq("CSTGMS20::GetVolume value", CSTGParamsOwner::sValueGetterTemp.value, 1202194794L);
	check_eq("CSTGMS20::GetVolume displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1202194794L);
	s->GetVolumeAMSIntensity(ctx);
	check_eq("CSTGMS20::GetVolumeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1021016602L);
	check_eq("CSTGMS20::GetVolumeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1021016602L);
	s->GetVolumeAMSSource(ctx);
	check_eq("CSTGMS20::GetVolumeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 98L);
	s->GetWheelAMSIntensity(ctx);
	check_eq("CSTGMS20::GetWheelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 2010659226L);
	check_eq("CSTGMS20::GetWheelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2010659226L);
	s->GetWheelAMSSource(ctx);
	check_eq("CSTGMS20::GetWheelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 22L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
