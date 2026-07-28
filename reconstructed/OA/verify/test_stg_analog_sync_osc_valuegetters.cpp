// SPDX-License-Identifier: GPL-2.0
/*
 * test_stg_analog_sync_osc_valuegetters.cpp  -  KAT for CSTGAnalogSyncOsc's Get*() family
 * (63 methods, see ../src/engine/stg_analog_sync_osc_valuegetters.cpp).
 *
 * Expected values computed here by a SEPARATE Python evaluator over the
 * same parsed (offset, ctx-index*stride, width, signed, dual) facts the
 * source file's own decoder used -- not by re-using the .cpp file's C
 * output strings -- against the same deterministic non-trivial byte
 * pattern as the CSTGString/CSTGOrganModelPatch/CSTGMS20 KATs (buf[i] =
 * (i*0x9f+0x37) & 0xff). ctx's own dynamic-index field (+0x4) is fixed at
 * 3 for every ctx-indexed getter, matching the established KAT convention.
 */

#include <cstdio>
#include <cstring>
#include "oa_stg_analog_sync_osc.h"

STGConvertedParam CSTGParamsOwner::sValueGetterTemp;

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld\n", label, got); return; }
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x300
static unsigned char g_buf[BUFSZ];
static unsigned char g_ctxbuf[0x40];

int main(void)
{
	printf("CSTGAnalogSyncOsc value-getter family known-answer test (63 methods)\n");
	printf("========================================================================\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_buf[i] = (unsigned char)(i*0x9f + 0x37);
	*(int *)(g_ctxbuf + 0x4) = 3;

	CSTGAnalogSyncOsc *s = (CSTGAnalogSyncOsc *)g_buf;
	CSTGPatchMessageContext &ctx = *(CSTGPatchMessageContext *)g_ctxbuf;

	s->GetBalanceM(ctx);
	check_eq("CSTGAnalogSyncOsc::GetBalanceM value", CSTGParamsOwner::sValueGetterTemp.value, 680127051L);
	check_eq("CSTGAnalogSyncOsc::GetBalanceM displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 680127051L);
	s->GetBalanceS(ctx);
	check_eq("CSTGAnalogSyncOsc::GetBalanceS value", CSTGParamsOwner::sValueGetterTemp.value, -768371467L);
	check_eq("CSTGAnalogSyncOsc::GetBalanceS displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -768371467L);
	s->GetEdge(ctx);
	check_eq("CSTGAnalogSyncOsc::GetEdge value", CSTGParamsOwner::sValueGetterTemp.value, 1741171082L);
	check_eq("CSTGAnalogSyncOsc::GetEdge displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1741171082L);
	s->GetFMAmount(ctx);
	check_eq("CSTGAnalogSyncOsc::GetFMAmount value", CSTGParamsOwner::sValueGetterTemp.value, 899086168L);
	check_eq("CSTGAnalogSyncOsc::GetFMAmount displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 899086168L);
	s->GetFMAmountAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetFMAmountAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1324190764L);
	check_eq("CSTGAnalogSyncOsc::GetFMAmountAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1324190764L);
	s->GetFMAmountAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetFMAmountAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 80L);
	s->GetInitialPhaseM(ctx);
	check_eq("CSTGAnalogSyncOsc::GetInitialPhaseM value", CSTGParamsOwner::sValueGetterTemp.value, -18921L);
	s->GetInitialPhaseS(ctx);
	check_eq("CSTGAnalogSyncOsc::GetInitialPhaseS value", CSTGParamsOwner::sValueGetterTemp.value, 15774L);
	s->GetLevelM(ctx);
	check_eq("CSTGAnalogSyncOsc::GetLevelM value", CSTGParamsOwner::sValueGetterTemp.value, 360109880L);
	check_eq("CSTGAnalogSyncOsc::GetLevelM displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 360109880L);
	s->GetLevelS(ctx);
	check_eq("CSTGAnalogSyncOsc::GetLevelS value", CSTGParamsOwner::sValueGetterTemp.value, -1088388638L);
	check_eq("CSTGAnalogSyncOsc::GetLevelS displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1088388638L);
	s->GetMasterBalanceAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetMasterBalanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1543149881L);
	check_eq("CSTGAnalogSyncOsc::GetMasterBalanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1543149881L);
	s->GetMasterBalanceAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetMasterBalanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 67L);
	s->GetMasterFreqOffset(ctx);
	check_eq("CSTGAnalogSyncOsc::GetMasterFreqOffset value", CSTGParamsOwner::sValueGetterTemp.value, 848557141L);
	check_eq("CSTGAnalogSyncOsc::GetMasterFreqOffset displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 848557141L);
	s->GetMasterLevelAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetMasterLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1846389836L);
	check_eq("CSTGAnalogSyncOsc::GetMasterLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1846389836L);
	s->GetMasterLevelAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetMasterLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 48L);
	s->GetMasterWaveformAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetMasterWaveformAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1117979749L);
	check_eq("CSTGAnalogSyncOsc::GetMasterWaveformAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1117979749L);
	s->GetMasterWaveformAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetMasterWaveformAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -31L);
	s->GetMasterWidthAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetMasterWidthAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -650470404L);
	check_eq("CSTGAnalogSyncOsc::GetMasterWidthAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -650470404L);
	s->GetMasterWidthAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetMasterWidthAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 120L);
	s->GetNoiseBalance(ctx);
	check_eq("CSTGAnalogSyncOsc::GetNoiseBalance value", CSTGParamsOwner::sValueGetterTemp.value, -802057485L);
	check_eq("CSTGAnalogSyncOsc::GetNoiseBalance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -802057485L);
	s->GetNoiseBalanceAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetNoiseBalanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1286409839L);
	check_eq("CSTGAnalogSyncOsc::GetNoiseBalanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1286409839L);
	s->GetNoiseBalanceAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetNoiseBalanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -21L);
	s->GetNoiseLevel(ctx);
	check_eq("CSTGAnalogSyncOsc::GetNoiseLevel value", CSTGParamsOwner::sValueGetterTemp.value, -1122074656L);
	check_eq("CSTGAnalogSyncOsc::GetNoiseLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1122074656L);
	s->GetNoiseLevelAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetNoiseLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 966458204L);
	check_eq("CSTGAnalogSyncOsc::GetNoiseLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 966458204L);
	s->GetNoiseLevelAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetNoiseLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -40L);
	s->GetNoisePhaseInvert(ctx);
	check_eq("CSTGAnalogSyncOsc::GetNoisePhaseInvert value", CSTGParamsOwner::sValueGetterTemp.value, 1421153911L);
	check_eq("CSTGAnalogSyncOsc::GetNoisePhaseInvert displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1421153911L);
	s->GetPhaseInvertM(ctx);
	check_eq("CSTGAnalogSyncOsc::GetPhaseInvertM value", CSTGParamsOwner::sValueGetterTemp.value, -1408405809L);
	check_eq("CSTGAnalogSyncOsc::GetPhaseInvertM displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1408405809L);
	s->GetPhaseInvertS(ctx);
	check_eq("CSTGAnalogSyncOsc::GetPhaseInvertS value", CSTGParamsOwner::sValueGetterTemp.value, 1454839929L);
	check_eq("CSTGAnalogSyncOsc::GetPhaseInvertS displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1454839929L);
	s->GetRingModBalance(ctx);
	check_eq("CSTGAnalogSyncOsc::GetRingModBalance value", CSTGParamsOwner::sValueGetterTemp.value, 646441033L);
	check_eq("CSTGAnalogSyncOsc::GetRingModBalance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 646441033L);
	s->GetRingModBalanceAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetRingModBalanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1576835899L);
	check_eq("CSTGAnalogSyncOsc::GetRingModBalanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1576835899L);
	s->GetRingModBalanceAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetRingModBalanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 65L);
	s->GetRingModCarrierSelect(ctx);
	check_eq("CSTGAnalogSyncOsc::GetRingModCarrierSelect value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetRingModLevel(ctx);
	check_eq("CSTGAnalogSyncOsc::GetRingModLevel value", CSTGParamsOwner::sValueGetterTemp.value, 326423862L);
	check_eq("CSTGAnalogSyncOsc::GetRingModLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 326423862L);
	s->GetRingModLevelAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetRingModLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -1880075854L);
	check_eq("CSTGAnalogSyncOsc::GetRingModLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1880075854L);
	s->GetRingModLevelAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetRingModLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 46L);
	s->GetRingModMode(ctx);
	check_eq("CSTGAnalogSyncOsc::GetRingModMode value", CSTGParamsOwner::sValueGetterTemp.value, 45L);
	s->GetRingModModulatorSelect(ctx);
	check_eq("CSTGAnalogSyncOsc::GetRingModModulatorSelect value", CSTGParamsOwner::sValueGetterTemp.value, 1L);
	s->GetRingModPhaseInvert(ctx);
	check_eq("CSTGAnalogSyncOsc::GetRingModPhaseInvert value", CSTGParamsOwner::sValueGetterTemp.value, -1442091827L);
	check_eq("CSTGAnalogSyncOsc::GetRingModPhaseInvert displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1442091827L);
	s->GetSlaveBalanceAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSlaveBalanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1320095857L);
	check_eq("CSTGAnalogSyncOsc::GetSlaveBalanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1320095857L);
	s->GetSlaveBalanceAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSlaveBalanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -19L);
	s->GetSlaveFreqOffset(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSlaveFreqOffset value", CSTGParamsOwner::sValueGetterTemp.value, -1189446692L);
	check_eq("CSTGAnalogSyncOsc::GetSlaveFreqOffset displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -1189446692L);
	s->GetSlaveLevelAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSlaveLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1000144222L);
	check_eq("CSTGAnalogSyncOsc::GetSlaveLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1000144222L);
	s->GetSlaveLevelAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSlaveLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -38L);
	s->GetSlaveWaveformAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSlaveWaveformAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -919958548L);
	check_eq("CSTGAnalogSyncOsc::GetSlaveWaveformAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -919958548L);
	s->GetSlaveWaveformAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSlaveWaveformAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, 104L);
	s->GetSlaveWidthAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSlaveWidthAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, 1623270019L);
	check_eq("CSTGAnalogSyncOsc::GetSlaveWidthAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1623270019L);
	s->GetSlaveWidthAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSlaveWidthAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -1L);
	s->GetSubOscAudioInModeSelect(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSubOscAudioInModeSelect value", CSTGParamsOwner::sValueGetterTemp.value, 0L);
	s->GetSubOscBalance(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSubOscBalance value", CSTGParamsOwner::sValueGetterTemp.value, 2094874271L);
	check_eq("CSTGAnalogSyncOsc::GetSubOscBalance displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 2094874271L);
	s->GetSubOscBalanceAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSubOscBalanceAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -128337381L);
	check_eq("CSTGAnalogSyncOsc::GetSubOscBalanceAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -128337381L);
	s->GetSubOscBalanceAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSubOscBalanceAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -105L);
	s->GetSubOscLevel(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSubOscLevel value", CSTGParamsOwner::sValueGetterTemp.value, 1774857100L);
	check_eq("CSTGAnalogSyncOsc::GetSubOscLevel displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1774857100L);
	s->GetSubOscLevelAMSIntensity(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSubOscLevelAMSIntensity value", CSTGParamsOwner::sValueGetterTemp.value, -448354552L);
	check_eq("CSTGAnalogSyncOsc::GetSubOscLevelAMSIntensity displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -448354552L);
	s->GetSubOscLevelAMSSource(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSubOscLevelAMSSource value", CSTGParamsOwner::sValueGetterTemp.value, -124L);
	s->GetSubOscPhaseInvert(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSubOscPhaseInvert value", CSTGParamsOwner::sValueGetterTemp.value, 6406691L);
	check_eq("CSTGAnalogSyncOsc::GetSubOscPhaseInvert displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 6406691L);
	s->GetSubOscWaveform(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSubOscWaveform value", CSTGParamsOwner::sValueGetterTemp.value, 142L);
	s->GetSyncEnable(ctx);
	check_eq("CSTGAnalogSyncOsc::GetSyncEnable value", CSTGParamsOwner::sValueGetterTemp.value, 239L);
	s->GetWaveformM(ctx);
	check_eq("CSTGAnalogSyncOsc::GetWaveformM value", CSTGParamsOwner::sValueGetterTemp.value, -970487575L);
	check_eq("CSTGAnalogSyncOsc::GetWaveformM displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -970487575L);
	s->GetWaveformS(ctx);
	check_eq("CSTGAnalogSyncOsc::GetWaveformS value", CSTGParamsOwner::sValueGetterTemp.value, 1303252848L);
	check_eq("CSTGAnalogSyncOsc::GetWaveformS displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1303252848L);
	s->GetWaveformSelectM(ctx);
	check_eq("CSTGAnalogSyncOsc::GetWaveformSelectM value", CSTGParamsOwner::sValueGetterTemp.value, 74L);
	s->GetWaveformSelectS(ctx);
	check_eq("CSTGAnalogSyncOsc::GetWaveformSelectS value", CSTGParamsOwner::sValueGetterTemp.value, 209L);
	s->GetWidthM(ctx);
	check_eq("CSTGAnalogSyncOsc::GetWidthM value", CSTGParamsOwner::sValueGetterTemp.value, 1572740992L);
	check_eq("CSTGAnalogSyncOsc::GetWidthM displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, 1572740992L);
	s->GetWidthS(ctx);
	check_eq("CSTGAnalogSyncOsc::GetWidthS value", CSTGParamsOwner::sValueGetterTemp.value, -465197561L);
	check_eq("CSTGAnalogSyncOsc::GetWidthS displayValue", CSTGParamsOwner::sValueGetterTemp.displayValue, -465197561L);

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
