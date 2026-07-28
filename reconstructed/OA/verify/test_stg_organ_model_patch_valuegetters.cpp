// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_organ_model_patch_valuegetters.cpp  -  KAT for
 * CSTGOrganModelPatch's Get*() family (101 methods, see
 * ../src/engine/stg_organ_model_patch_valuegetters.cpp).
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed (offset, width, signed, invert, dual) facts the source
 * file's own decoder used -- not by re-using stg_organ_model_patch_
 * valuegetters.cpp's C output strings -- against the same deterministic
 * non-trivial byte pattern as the CSTGString KAT (buf[i] = (i*0x9f+0x37)
 * & 0xff).
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_organ_model_patch.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld\n", label, got); return; }
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x140
static unsigned char g_buf[BUFSZ];

int main(void)
{
	printf("CSTGOrganModelPatch value-getter family known-answer test (101 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);

	CSTGOrganModelPatch *s = (CSTGOrganModelPatch *)g_buf;
	CSTGPatchMessageContext ctxbuf;
	memset(&ctxbuf, 0, sizeof(ctxbuf));

	s->GetAmpGain(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetAmpGain value", CSTGParamsOwner::sValueGetterTemp.value, -128337381L);
	check_eq("CSTGOrganModelPatch::GetAmpGain displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -128337381L);
	s->GetAmpGainAMSIntensity(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetAmpGainAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1960130199L);
	check_eq("CSTGOrganModelPatch::GetAmpGainAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1960130199L);
	s->GetAmpGainAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetAmpGainAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 19L);
	s->GetAmpRotaryVersion(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetAmpRotaryVersion value", CSTGParamsOwner::sValueGetterTemp.value, -35L);
	s->GetAmpToneBass(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetAmpToneBass value", CSTGParamsOwner::sValueGetterTemp.value, 1690642055L);
	check_eq("CSTGOrganModelPatch::GetAmpToneBass displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1690642055L);
	s->GetAmpToneMiddle(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetAmpToneMiddle value", CSTGParamsOwner::sValueGetterTemp.value, -397825525L);
	check_eq("CSTGOrganModelPatch::GetAmpToneMiddle displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -397825525L);
	s->GetAmpToneTreble(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetAmpToneTreble value", CSTGParamsOwner::sValueGetterTemp.value, 1825386127L);
	check_eq("CSTGOrganModelPatch::GetAmpToneTreble displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1825386127L);
	s->GetAmpType(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetAmpType value", CSTGParamsOwner::sValueGetterTemp.value, 124L);
	s->GetEXModeEnable(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetEXModeEnable value", CSTGParamsOwner::sValueGetterTemp.value, 171L);
	s->GetExpressionAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetExpressionAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -97L);
	s->GetExpressionLevel(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetExpressionLevel value", CSTGParamsOwner::sValueGetterTemp.value, 6406691L);
	check_eq("CSTGOrganModelPatch::GetExpressionLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 6406691L);
	s->GetExpressionMinimum(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetExpressionMinimum value", CSTGParamsOwner::sValueGetterTemp.value, -2065348953L);
	check_eq("CSTGOrganModelPatch::GetExpressionMinimum displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2065348953L);
	s->GetExpressionMode(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetExpressionMode value", CSTGParamsOwner::sValueGetterTemp.value, 62L);
	s->GetKeyOffClickLevel(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetKeyOffClickLevel value", CSTGParamsOwner::sValueGetterTemp.value, -465197561L);
	check_eq("CSTGOrganModelPatch::GetKeyOffClickLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -465197561L);
	s->GetKeyOnClickLevel(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetKeyOnClickLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1758014091L);
	check_eq("CSTGOrganModelPatch::GetKeyOnClickLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1758014091L);
	s->GetLeakageLevel(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetLeakageLevel value", CSTGParamsOwner::sValueGetterTemp.value, -2132720989L);
	check_eq("CSTGOrganModelPatch::GetLeakageLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2132720989L);
	s->GetNoiseLevel(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetNoiseLevel value", CSTGParamsOwner::sValueGetterTemp.value, -60965345L);
	check_eq("CSTGOrganModelPatch::GetNoiseLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -60965345L);
	s->GetOutputLevel(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetOutputLevel value", CSTGParamsOwner::sValueGetterTemp.value, -532569597L);
	check_eq("CSTGOrganModelPatch::GetOutputLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -532569597L);
	s->GetOutputLevelAMSIntensity(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetOutputLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1555897983L);
	check_eq("CSTGOrganModelPatch::GetOutputLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1555897983L);
	s->GetOutputLevelAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetOutputLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -5L);
	s->GetOvertoneLevel(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetOvertoneLevel value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTGOrganModelPatch::GetOvertoneLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);
	s->GetPercDecaySwitch(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercDecaySwitch value", CSTGParamsOwner::sValueGetterTemp.value, 239L);
	s->GetPercDecaySwitchAMSMode(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercDecaySwitchAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, 45L);
	s->GetPercDecaySwitchAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercDecaySwitchAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -114L);
	s->GetPercEnable(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercEnable value", CSTGParamsOwner::sValueGetterTemp.value, 131L);
	s->GetPercEnableAMSMode(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercEnableAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, -63L);
	s->GetPercEnableAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercEnableAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 34L);
	s->GetPercFastDecayTime(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercFastDecayTime value", CSTGParamsOwner::sValueGetterTemp.value, 1219037803L);
	check_eq("CSTGOrganModelPatch::GetPercFastDecayTime displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1219037803L);
	s->GetPercHarmonicSwitch(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercHarmonicSwitch value", CSTGParamsOwner::sValueGetterTemp.value, 99L);
	s->GetPercHarmonicSwitchAMSMode(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercHarmonicSwitchAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, -95L);
	s->GetPercHarmonicSwitchAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercHarmonicSwitchAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 2L);
	s->GetPercLevelSwitch(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercLevelSwitch value", CSTGParamsOwner::sValueGetterTemp.value, 97L);
	s->GetPercLevelSwitchAMSMode(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercLevelSwitchAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, -98L);
	s->GetPercLevelSwitchAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercLevelSwitchAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -1L);
	s->GetPercLoudDBAttenuation(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercLoudDBAttenuation value", CSTGParamsOwner::sValueGetterTemp.value, 1353781875L);
	check_eq("CSTGOrganModelPatch::GetPercLoudDBAttenuation displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1353781875L);
	s->GetPercLoudLevel(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercLoudLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1488525947L);
	check_eq("CSTGOrganModelPatch::GetPercLoudLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1488525947L);
	s->GetPercSlowDecayTime(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercSlowDecayTime value", CSTGParamsOwner::sValueGetterTemp.value, -1004173593L);
	check_eq("CSTGOrganModelPatch::GetPercSlowDecayTime displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1004173593L);
	s->GetPercSoftLevel(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPercSoftLevel value", CSTGParamsOwner::sValueGetterTemp.value, -734685449L);
	check_eq("CSTGOrganModelPatch::GetPercSoftLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -734685449L);
	s->GetPitchBendDown(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPitchBendDown value", CSTGParamsOwner::sValueGetterTemp.value, -330453489L);
	check_eq("CSTGOrganModelPatch::GetPitchBendDown displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -330453489L);
	s->GetPitchBendUp(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetPitchBendUp value", CSTGParamsOwner::sValueGetterTemp.value, 1892758163L);
	check_eq("CSTGOrganModelPatch::GetPitchBendUp displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1892758163L);
	s->GetRotaryFast(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryFast value", CSTGParamsOwner::sValueGetterTemp.value, 200L);
	s->GetRotaryFastAMSMode(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryFastAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, 6L);
	s->GetRotaryFastAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryFastAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 103L);
	s->GetRotaryFastOverridesStop(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryFastOverridesStop value", CSTGParamsOwner::sValueGetterTemp.value, 165L);
	s->GetRotaryHornDownTransit(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryHornDownTransit value", CSTGParamsOwner::sValueGetterTemp.value, 73778727L);
	check_eq("CSTGOrganModelPatch::GetRotaryHornDownTransit displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 73778727L);
	s->GetRotaryHornFastSpeed(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryHornFastSpeed value", CSTGParamsOwner::sValueGetterTemp.value, 1016987231L);
	check_eq("CSTGOrganModelPatch::GetRotaryHornFastSpeed displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1016987231L);
	s->GetRotaryHornMicSpread(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryHornMicSpread value", CSTGParamsOwner::sValueGetterTemp.value, -1475777845L);
	check_eq("CSTGOrganModelPatch::GetRotaryHornMicSpread displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1475777845L);
	s->GetRotaryHornRotorBalance(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryHornRotorBalance value", CSTGParamsOwner::sValueGetterTemp.value, 1892758163L);
	check_eq("CSTGOrganModelPatch::GetRotaryHornRotorBalance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1892758163L);
	s->GetRotaryHornSlowSpeed(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryHornSlowSpeed value", CSTGParamsOwner::sValueGetterTemp.value, -1071545629L);
	check_eq("CSTGOrganModelPatch::GetRotaryHornSlowSpeed displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1071545629L);
	s->GetRotaryHornStartTransit(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryHornStartTransit value", CSTGParamsOwner::sValueGetterTemp.value, 208522799L);
	check_eq("CSTGOrganModelPatch::GetRotaryHornStartTransit displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 208522799L);
	s->GetRotaryHornStopPhase(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryHornStopPhase value", CSTGParamsOwner::sValueGetterTemp.value, -1610521917L);
	s->GetRotaryHornStopTransit(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryHornStopTransit value", CSTGParamsOwner::sValueGetterTemp.value, 343266871L);
	check_eq("CSTGOrganModelPatch::GetRotaryHornStopTransit displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 343266871L);
	s->GetRotaryHornUpTransit(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryHornUpTransit value", CSTGParamsOwner::sValueGetterTemp.value, -60965345L);
	check_eq("CSTGOrganModelPatch::GetRotaryHornUpTransit displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -60965345L);
	s->GetRotaryOffOutput(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryOffOutput value", CSTGParamsOwner::sValueGetterTemp.value, 77L);
	s->GetRotaryOn(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryOn value", CSTGParamsOwner::sValueGetterTemp.value, 14L);
	s->GetRotaryOnAMSMode(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryOnAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, 76L);
	s->GetRotaryOnAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryOnAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -83L);
	s->GetRotaryRotorDownTransit(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorDownTransit value", CSTGParamsOwner::sValueGetterTemp.value, -1997976917L);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorDownTransit displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1997976917L);
	s->GetRotaryRotorFastSpeed(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorFastSpeed value", CSTGParamsOwner::sValueGetterTemp.value, 747499087L);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorFastSpeed displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 747499087L);
	s->GetRotaryRotorMicSpread(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorMicSpread value", CSTGParamsOwner::sValueGetterTemp.value, 612755015L);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorMicSpread displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 612755015L);
	s->GetRotaryRotorSlowSpeed(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorSlowSpeed value", CSTGParamsOwner::sValueGetterTemp.value, -1341033773L);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorSlowSpeed displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1341033773L);
	s->GetRotaryRotorStartTransit(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorStartTransit value", CSTGParamsOwner::sValueGetterTemp.value, -1863232845L);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorStartTransit displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1863232845L);
	s->GetRotaryRotorStopPhase(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorStopPhase value", CSTGParamsOwner::sValueGetterTemp.value, 478010943L);
	s->GetRotaryRotorStopTransit(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorStopTransit value", CSTGParamsOwner::sValueGetterTemp.value, -1728488773L);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorStopTransit displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1728488773L);
	s->GetRotaryRotorUpTransit(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorUpTransit value", CSTGParamsOwner::sValueGetterTemp.value, -2132720989L);
	check_eq("CSTGOrganModelPatch::GetRotaryRotorUpTransit displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -2132720989L);
	s->GetRotarySpeakerSim(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotarySpeakerSim value", CSTGParamsOwner::sValueGetterTemp.value, 15L);
	s->GetRotarySpeakerSimType(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotarySpeakerSimType value", CSTGParamsOwner::sValueGetterTemp.value, -82L);
	s->GetRotaryStop(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryStop value", CSTGParamsOwner::sValueGetterTemp.value, 235L);
	s->GetRotaryStopAMSMode(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryStopAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, 41L);
	s->GetRotaryStopAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryStopAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -118L);
	s->GetRotaryWetDry(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryWetDry value", CSTGParamsOwner::sValueGetterTemp.value, 1421153911L);
	check_eq("CSTGOrganModelPatch::GetRotaryWetDry displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1421153911L);
	s->GetRotaryWetDryAMSIntensity(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryWetDryAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -802057485L);
	check_eq("CSTGOrganModelPatch::GetRotaryWetDryAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -802057485L);
	s->GetRotaryWetDryAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetRotaryWetDryAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 111L);
	s->GetSplitEnable(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetSplitEnable value", CSTGParamsOwner::sValueGetterTemp.value, 155L);
	s->GetSplitEnableAMSMode(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetSplitEnableAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, -39L);
	s->GetSplitEnableAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetSplitEnableAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 58L);
	s->GetVCDepth(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCDepth value", CSTGParamsOwner::sValueGetterTemp.value, -1661116737L);
	check_eq("CSTGOrganModelPatch::GetVCDepth displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1661116737L);
	s->GetVCDepthAMSIntensity(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCDepthAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1357876782L);
	check_eq("CSTGOrganModelPatch::GetVCDepthAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1357876782L);
	s->GetVCDepthAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCDepthAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 78L);
	s->GetVCLowerEnable(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCLowerEnable value", CSTGParamsOwner::sValueGetterTemp.value, 29L);
	s->GetVCLowerEnableAMSMode(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCLowerEnableAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, 91L);
	s->GetVCLowerEnableAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCLowerEnableAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -68L);
	s->GetVCMix(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCMix value", CSTGParamsOwner::sValueGetterTemp.value, 545382979L);
	check_eq("CSTGOrganModelPatch::GetVCMix displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 545382979L);
	s->GetVCMixAMSIntensity(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCMixAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1795860809L);
	check_eq("CSTGOrganModelPatch::GetVCMixAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1795860809L);
	s->GetVCMixAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCMixAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 51L);
	s->GetVCMode(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCMode value", CSTGParamsOwner::sValueGetterTemp.value, 83L);
	s->GetVCSpeed(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCSpeed value", CSTGParamsOwner::sValueGetterTemp.value, 410638907L);
	check_eq("CSTGOrganModelPatch::GetVCSpeed displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 410638907L);
	s->GetVCSpeedAMSIntensity(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCSpeedAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -903115539L);
	check_eq("CSTGOrganModelPatch::GetVCSpeedAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -903115539L);
	s->GetVCSpeedAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCSpeedAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 105L);
	s->GetVCTrim(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCTrim value", CSTGParamsOwner::sValueGetterTemp.value, -1273661737L);
	check_eq("CSTGOrganModelPatch::GetVCTrim displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1273661737L);
	s->GetVCType(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCType value", CSTGParamsOwner::sValueGetterTemp.value, -1408405809L);
	s->GetVCTypeAMSIntensity(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCTypeAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 680127051L);
	check_eq("CSTGOrganModelPatch::GetVCTypeAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 680127051L);
	s->GetVCTypeAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCTypeAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -57L);
	s->GetVCUpperEnable(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCUpperEnable value", CSTGParamsOwner::sValueGetterTemp.value, 64L);
	s->GetVCUpperEnableAMSMode(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCUpperEnableAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, 126L);
	s->GetVCUpperEnableAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetVCUpperEnableAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -33L);
	s->GetWheelBrakeEnable(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetWheelBrakeEnable value", CSTGParamsOwner::sValueGetterTemp.value, 120L);
	s->GetWheelBrakeEnableAMSMode(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetWheelBrakeEnableAMSMode value", CSTGParamsOwner::sValueGetterTemp.value, -74L);
	s->GetWheelBrakeEnableAMSSource(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetWheelBrakeEnableAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 23L);
	s->GetWheelBrakeSpeed(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetWheelBrakeSpeed value", CSTGParamsOwner::sValueGetterTemp.value, 85L);
	s->GetWheelType(ctxbuf);
	check_eq("CSTGOrganModelPatch::GetWheelType value", CSTGParamsOwner::sValueGetterTemp.value, 74L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
