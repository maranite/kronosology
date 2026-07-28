// SPDX-License-Identifier: GPL-2.0
/*
 * test_ckg_seq_backup.cpp  -  KAT for CKGSeqBackupCommonParam / CKGSeqBackupModuleParam
 * (see ../src/engine/karma_seq_backup.cpp).
 *
 * For all 195 generic-shape Set* methods, the expected value is computed here
 * by a SEPARATE evaluator (embedded as a literal constant per case, generated
 * from the SAME parsed (base, index*stride, disp, width, signed, shift, mask)
 * facts the source file's own decoder used -- but via an independent Python
 * arithmetic path, not by re-using src/engine/karma_seq_backup.cpp's C output
 * strings), against a deterministic non-trivial byte pattern in the source/
 * default buffers (src[i] = (i*0x9f + 0x37) & 0xff, dflt[i] = ~src[i] & 0xff --
 * chosen so every individual bit position is independently distinguishable,
 * unlike an all-same-byte pattern). idx is fixed at 3 for every indexed setter.
 * SetLinkedSceneId/SetModCutoff (the 2 hand-written non-generic-shape methods)
 * and SetSolo (constant-0 store) get dedicated hand-derived checks. Chosen index
 * 3 gives SetLinkedSceneId idx&1==1 (odd/high-nibble path); a second call with
 * idx=4 exercises the even/low-nibble path.
 */

#include <cstdio>
#include <cstring>
#include "oa_karma_seq_backup.h"

/*
 * Host-only mocks for the 3 CKGBankManager methods and the
 * CKGBankManager::ms_poInstance storage GetKarmaPerf{Common,Module}
 * ForSeqBackup() call into (real bodies belong to CKGBankManager, out of
 * scope for this batch -- see oa_engine_init.h's own note). Same
 * "host test provides mocks for out-of-scope dependencies" convention as
 * test_sk_stg_gate.cpp's own bankMgr[] stand-in, except that file links
 * the REAL sk_stg_gate.cpp (which owns CKGBankManager::ms_poInstance's
 * storage) -- this test deliberately does NOT link that unrelated file,
 * so it provides both the storage and the 2 mock method bodies itself.
 */
unsigned char *CKGBankManager::ms_poInstance;
static unsigned char *g_mockCommonPerf;
static unsigned char *g_mockModulePerf;
unsigned char *CKGBankManager::GetSeqKarmaPerfCommon(unsigned int)
{
	return g_mockCommonPerf;
}
unsigned char *CKGBankManager::GetSeqKarmaPerfModule(unsigned int)
{
	return g_mockModulePerf;
}

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-55s %ld\n", label, got); return; }
	printf("  FAIL  %-55s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x600
static unsigned char g_src[BUFSZ];
static unsigned char g_dflt[BUFSZ];

static void fill_buffers()
{
	for (unsigned int i = 0; i < BUFSZ; i++) {
		g_src[i] = (unsigned char)(i*0x9f + 0x37);
		g_dflt[i] = (unsigned char)(~g_src[i]);
	}
}

int main(void)
{
	printf("CKGSeqBackupCommonParam / CKGSeqBackupModuleParam known-answer test\n");
	printf("========================================================================\n");
	fill_buffers();

	CKGSeqBackupCommonParam common;
	common.m_source = g_src;
	common.m_default = g_dflt;
	common.m_index = 3;

	CKGSeqBackupModuleParam module;
	module.m_source = g_src;
	module.m_default = g_dflt;
	module.m_index = 3;

	common.SetTempo(); check_eq("CKGSeqBackupCommonParam::SetTempo", common.m_value, 54839L);
	common.SetTimeSig(); check_eq("CKGSeqBackupCommonParam::SetTimeSig", common.m_value, 20L);
	common.SetPadMode(); check_eq("CKGSeqBackupCommonParam::SetPadMode", common.m_value, 1L);
	common.SetModuleControl(); check_eq("CKGSeqBackupCommonParam::SetModuleControl", common.m_value, 5L);
	common.SetLatch(); check_eq("CKGSeqBackupCommonParam::SetLatch", common.m_value, 1L);
	common.SetOnOff(); check_eq("CKGSeqBackupCommonParam::SetOnOff", common.m_value, 0L);
	common.SetSwName(); check_eq("CKGSeqBackupCommonParam::SetSwName", common.m_value, 62354L);
	common.SetKnobName(); check_eq("CKGSeqBackupCommonParam::SetKnobName", common.m_value, 930L);
	common.SetNoteMapTableValue(); check_eq("CKGSeqBackupCommonParam::SetNoteMapTableValue", common.m_value, 86L);
	common.SetSceneChangeQuantizeValue(); check_eq("CKGSeqBackupCommonParam::SetSceneChangeQuantizeValue", common.m_value, 2L);
	common.SetDynMIDIInput(); check_eq("CKGSeqBackupCommonParam::SetDynMIDIInput", common.m_value, 1L);
	common.SetDynMIDIPolarity(); check_eq("CKGSeqBackupCommonParam::SetDynMIDIPolarity", common.m_value, 0L);
	common.SetDynMIDISource(); check_eq("CKGSeqBackupCommonParam::SetDynMIDISource", common.m_value, 96L);
	common.SetDynMIDIDest(); check_eq("CKGSeqBackupCommonParam::SetDynMIDIDest", common.m_value, 255L);
	common.SetDynMIDIUseA(); check_eq("CKGSeqBackupCommonParam::SetDynMIDIUseA", common.m_value, 0L);
	common.SetDynMIDIUseB(); check_eq("CKGSeqBackupCommonParam::SetDynMIDIUseB", common.m_value, 1L);
	common.SetDynMIDIUseC(); check_eq("CKGSeqBackupCommonParam::SetDynMIDIUseC", common.m_value, 1L);
	common.SetDynMIDIUseD(); check_eq("CKGSeqBackupCommonParam::SetDynMIDIUseD", common.m_value, 1L);
	common.SetDynMIDIUseLast(); check_eq("CKGSeqBackupCommonParam::SetDynMIDIUseLast", common.m_value, 1L);
	common.SetDynMIDIUseAction(); check_eq("CKGSeqBackupCommonParam::SetDynMIDIUseAction", common.m_value, 0L);
	common.SetDynMIDIUseTop(); check_eq("CKGSeqBackupCommonParam::SetDynMIDIUseTop", common.m_value, 61L);
	common.SetDynMIDIUseBottom(); check_eq("CKGSeqBackupCommonParam::SetDynMIDIUseBottom", common.m_value, 220L);
	common.SetRTParamGroup(); check_eq("CKGSeqBackupCommonParam::SetRTParamGroup", common.m_value, 5L);
	common.SetRTParamAssign(); check_eq("CKGSeqBackupCommonParam::SetRTParamAssign", common.m_value, 164L);
	common.SetRTParamA(); check_eq("CKGSeqBackupCommonParam::SetRTParamA", common.m_value, 0L);
	common.SetRTParamB(); check_eq("CKGSeqBackupCommonParam::SetRTParamB", common.m_value, 0L);
	common.SetRTParamC(); check_eq("CKGSeqBackupCommonParam::SetRTParamC", common.m_value, 1L);
	common.SetRTParamD(); check_eq("CKGSeqBackupCommonParam::SetRTParamD", common.m_value, 1L);
	common.SetRTParamPolarity(); check_eq("CKGSeqBackupCommonParam::SetRTParamPolarity", common.m_value, 4L);
	common.SetRTParamKnob(); check_eq("CKGSeqBackupCommonParam::SetRTParamKnob", common.m_value, -30L);
	common.SetRTParamMin(); check_eq("CKGSeqBackupCommonParam::SetRTParamMin", common.m_value, -8322L);
	common.SetRTParamMax(); check_eq("CKGSeqBackupCommonParam::SetRTParamMax", common.m_value, -24256L);
	common.SetRTParamValue(); check_eq("CKGSeqBackupCommonParam::SetRTParamValue", common.m_value, 25346L);
	common.SetChordMemNote1(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote1", common.m_value, -99L);
	common.SetChordMemNote2(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote2", common.m_value, -37L);
	common.SetChordMemNote3(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote3", common.m_value, 25L);
	common.SetChordMemNote4(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote4", common.m_value, 87L);
	common.SetChordMemNote5(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote5", common.m_value, -107L);
	common.SetChordMemNote6(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote6", common.m_value, -45L);
	common.SetChordMemNote7(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote7", common.m_value, 17L);
	common.SetChordMemNote8(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote8", common.m_value, 79L);
	common.SetChordMemNote1Vel(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote1Vel", common.m_value, 60L);
	common.SetChordMemNote2Vel(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote2Vel", common.m_value, 122L);
	common.SetChordMemNote3Vel(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote3Vel", common.m_value, 184L);
	common.SetChordMemNote4Vel(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote4Vel", common.m_value, 246L);
	common.SetChordMemNote5Vel(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote5Vel", common.m_value, 52L);
	common.SetChordMemNote6Vel(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote6Vel", common.m_value, 114L);
	common.SetChordMemNote7Vel(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote7Vel", common.m_value, 176L);
	common.SetChordMemNote8Vel(); check_eq("CKGSeqBackupCommonParam::SetChordMemNote8Vel", common.m_value, 238L);
	common.m_value = -12345; common.SetChordMemVelocity(); check_eq("CKGSeqBackupCommonParam::SetChordMemVelocity (real no-op)", common.m_value, -12345);
	common.SetChordMemChannel(); check_eq("CKGSeqBackupCommonParam::SetChordMemChannel", common.m_value, 44L);
	common.SetScene(); check_eq("CKGSeqBackupCommonParam::SetScene", common.m_value, 5L);
	common.SetSw1Value(); check_eq("CKGSeqBackupCommonParam::SetSw1Value", common.m_value, 1L);
	common.SetSw2Value(); check_eq("CKGSeqBackupCommonParam::SetSw2Value", common.m_value, 0L);
	common.SetSw3Value(); check_eq("CKGSeqBackupCommonParam::SetSw3Value", common.m_value, 0L);
	common.SetSw4Value(); check_eq("CKGSeqBackupCommonParam::SetSw4Value", common.m_value, 1L);
	common.SetSw5Value(); check_eq("CKGSeqBackupCommonParam::SetSw5Value", common.m_value, 1L);
	common.SetSw6Value(); check_eq("CKGSeqBackupCommonParam::SetSw6Value", common.m_value, 1L);
	common.SetSw7Value(); check_eq("CKGSeqBackupCommonParam::SetSw7Value", common.m_value, 1L);
	common.SetSw8Value(); check_eq("CKGSeqBackupCommonParam::SetSw8Value", common.m_value, 0L);
	common.SetKnob1Value(); check_eq("CKGSeqBackupCommonParam::SetKnob1Value", common.m_value, 218L);
	common.SetKnob2Value(); check_eq("CKGSeqBackupCommonParam::SetKnob2Value", common.m_value, 59L);
	common.SetKnob3Value(); check_eq("CKGSeqBackupCommonParam::SetKnob3Value", common.m_value, 156L);
	common.SetKnob4Value(); check_eq("CKGSeqBackupCommonParam::SetKnob4Value", common.m_value, 253L);
	common.SetKnob5Value(); check_eq("CKGSeqBackupCommonParam::SetKnob5Value", common.m_value, 94L);
	common.SetKnob6Value(); check_eq("CKGSeqBackupCommonParam::SetKnob6Value", common.m_value, 191L);
	common.SetKnob7Value(); check_eq("CKGSeqBackupCommonParam::SetKnob7Value", common.m_value, 32L);
	common.SetKnob8Value(); check_eq("CKGSeqBackupCommonParam::SetKnob8Value", common.m_value, 129L);
	common.SetDTRun(); check_eq("CKGSeqBackupCommonParam::SetDTRun", common.m_value, 0L);
	module.SetGE(); check_eq("CKGSeqBackupModuleParam::SetGE", module.m_value, -10697L);
	module.SetSolo(); check_eq("CKGSeqBackupModuleParam::SetSolo", module.m_value, 0L);
	module.SetInputCh(); check_eq("CKGSeqBackupModuleParam::SetInputCh", module.m_value, 21L);
	module.SetOutputCh(); check_eq("CKGSeqBackupModuleParam::SetOutputCh", module.m_value, 20L);
	module.SetKeyTop(); check_eq("CKGSeqBackupModuleParam::SetKeyTop", module.m_value, 82L);
	module.SetKeyBottom(); check_eq("CKGSeqBackupModuleParam::SetKeyBottom", module.m_value, 241L);
	module.SetRxBend(); check_eq("CKGSeqBackupModuleParam::SetRxBend", module.m_value, 1L);
	module.SetRxAfter(); check_eq("CKGSeqBackupModuleParam::SetRxAfter", module.m_value, 1L);
	module.SetRxDamper(); check_eq("CKGSeqBackupModuleParam::SetRxDamper", module.m_value, 0L);
	module.SetRxJSYP(); check_eq("CKGSeqBackupModuleParam::SetRxJSYP", module.m_value, 0L);
	module.SetRxJSYM(); check_eq("CKGSeqBackupModuleParam::SetRxJSYM", module.m_value, 0L);
	module.SetRxRibbon(); check_eq("CKGSeqBackupModuleParam::SetRxRibbon", module.m_value, 0L);
	module.SetRxOther(); check_eq("CKGSeqBackupModuleParam::SetRxOther", module.m_value, 1L);
	module.SetTxBend(); check_eq("CKGSeqBackupModuleParam::SetTxBend", module.m_value, 0L);
	module.SetTxCCA(); check_eq("CKGSeqBackupModuleParam::SetTxCCA", module.m_value, 0L);
	module.SetTxCCB(); check_eq("CKGSeqBackupModuleParam::SetTxCCB", module.m_value, 1L);
	module.SetTxEnv1(); check_eq("CKGSeqBackupModuleParam::SetTxEnv1", module.m_value, 0L);
	module.SetTxEnv2(); check_eq("CKGSeqBackupModuleParam::SetTxEnv2", module.m_value, 0L);
	module.SetTxEnv3(); check_eq("CKGSeqBackupModuleParam::SetTxEnv3", module.m_value, 0L);
	module.SetTxNote(); check_eq("CKGSeqBackupModuleParam::SetTxNote", module.m_value, 0L);
	module.SetTxWaveform(); check_eq("CKGSeqBackupModuleParam::SetTxWaveform", module.m_value, 0L);
	module.SetTranspose(); check_eq("CKGSeqBackupModuleParam::SetTranspose", module.m_value, -77L);
	module.SetCollapse(); check_eq("CKGSeqBackupModuleParam::SetCollapse", module.m_value, 2L);
	module.SetForceRangeWrap(); check_eq("CKGSeqBackupModuleParam::SetForceRangeWrap", module.m_value, 229L);
	module.SetTZoneBypass(); check_eq("CKGSeqBackupModuleParam::SetTZoneBypass", module.m_value, 0L);
	module.SetDelayTime(); check_eq("CKGSeqBackupModuleParam::SetDelayTime", module.m_value, 3181L);
	module.SetDelayMode(); check_eq("CKGSeqBackupModuleParam::SetDelayMode", module.m_value, 171L);
	module.SetRun(); check_eq("CKGSeqBackupModuleParam::SetRun", module.m_value, 0L);
	module.SetKbdInZone(); check_eq("CKGSeqBackupModuleParam::SetKbdInZone", module.m_value, 0L);
	module.SetKbdOutZone(); check_eq("CKGSeqBackupModuleParam::SetKbdOutZone", module.m_value, 1L);
	module.SetQuantize(); check_eq("CKGSeqBackupModuleParam::SetQuantize", module.m_value, 1L);
	module.SetThru(); check_eq("CKGSeqBackupModuleParam::SetThru", module.m_value, 0L);
	module.SetRootPosition(); check_eq("CKGSeqBackupModuleParam::SetRootPosition", module.m_value, 0L);
	module.SetGenCC(); check_eq("CKGSeqBackupModuleParam::SetGenCC", module.m_value, -109L);
	module.SetGenCCValue(); check_eq("CKGSeqBackupModuleParam::SetGenCCValue", module.m_value, 50L);
	module.SetNoteTrig(); check_eq("CKGSeqBackupModuleParam::SetNoteTrig", module.m_value, 8L);
	module.SetNoteLatch(); check_eq("CKGSeqBackupModuleParam::SetNoteLatch", module.m_value, 0L);
	module.SetEnv1Trig(); check_eq("CKGSeqBackupModuleParam::SetEnv1Trig", module.m_value, 7L);
	module.SetEnv2Trig(); check_eq("CKGSeqBackupModuleParam::SetEnv2Trig", module.m_value, 6L);
	module.SetEnv3Trig(); check_eq("CKGSeqBackupModuleParam::SetEnv3Trig", module.m_value, 5L);
	module.SetEnv1Latch(); check_eq("CKGSeqBackupModuleParam::SetEnv1Latch", module.m_value, 2L);
	module.SetEnv2Latch(); check_eq("CKGSeqBackupModuleParam::SetEnv2Latch", module.m_value, 12L);
	module.SetEnv3Latch(); check_eq("CKGSeqBackupModuleParam::SetEnv3Latch", module.m_value, 6L);
	module.SetClkAdvMode(); check_eq("CKGSeqBackupModuleParam::SetClkAdvMode", module.m_value, 2L);
	module.SetClkAdvSize(); check_eq("CKGSeqBackupModuleParam::SetClkAdvSize", module.m_value, 2L);
	module.SetClkAdvCtrig(); check_eq("CKGSeqBackupModuleParam::SetClkAdvCtrig", module.m_value, 15L);
	module.SetClkAdvVSence(); check_eq("CKGSeqBackupModuleParam::SetClkAdvVSence", module.m_value, 206L);
	module.SetTrigModule(); check_eq("CKGSeqBackupModuleParam::SetTrigModule", module.m_value, 10L);
	module.SetModPercent(); check_eq("CKGSeqBackupModuleParam::SetModPercent", module.m_value, 233L);
	module.SetKIZoneTrans(); check_eq("CKGSeqBackupModuleParam::SetKIZoneTrans", module.m_value, -128L);
	module.SetKOZoneTrans(); check_eq("CKGSeqBackupModuleParam::SetKOZoneTrans", module.m_value, 31L);
	module.SetRndRhythm(); check_eq("CKGSeqBackupModuleParam::SetRndRhythm", module.m_value, 2L);
	module.SetRndDuration(); check_eq("CKGSeqBackupModuleParam::SetRndDuration", module.m_value, 0L);
	module.SetRndNote(); check_eq("CKGSeqBackupModuleParam::SetRndNote", module.m_value, 0L);
	module.SetRndCluster(); check_eq("CKGSeqBackupModuleParam::SetRndCluster", module.m_value, 1L);
	module.SetRndVelocity(); check_eq("CKGSeqBackupModuleParam::SetRndVelocity", module.m_value, 1L);
	module.SetRndPan(); check_eq("CKGSeqBackupModuleParam::SetRndPan", module.m_value, 0L);
	module.SetRndDrum(); check_eq("CKGSeqBackupModuleParam::SetRndDrum", module.m_value, 2L);
	module.SetRndWaveform(); check_eq("CKGSeqBackupModuleParam::SetRndWaveform", module.m_value, 3L);
	module.SetSeed(); check_eq("CKGSeqBackupModuleParam::SetSeed", module.m_value, 58L);
	module.SetFreezeLoop(); check_eq("CKGSeqBackupModuleParam::SetFreezeLoop", module.m_value, 62L);
	module.SetFreezeRetrig(); check_eq("CKGSeqBackupModuleParam::SetFreezeRetrig", module.m_value, 1L);
	module.SetUseGChAlso(); check_eq("CKGSeqBackupModuleParam::SetUseGChAlso", module.m_value, 0L);
	module.SetNoteMap(); check_eq("CKGSeqBackupModuleParam::SetNoteMap", module.m_value, 167L);
	module.SetNoteMapTranspose(); check_eq("CKGSeqBackupModuleParam::SetNoteMapTranspose", module.m_value, -124L);
	module.SetNoteMapOnMode(); check_eq("CKGSeqBackupModuleParam::SetNoteMapOnMode", module.m_value, 2L);
	module.SetNoteMapChdTrack(); check_eq("CKGSeqBackupModuleParam::SetNoteMapChdTrack", module.m_value, 1L);
	module.SetNoteMapKbdTrack(); check_eq("CKGSeqBackupModuleParam::SetNoteMapKbdTrack", module.m_value, 0L);
	module.SetUseNoteOffs(); check_eq("CKGSeqBackupModuleParam::SetUseNoteOffs", module.m_value, 1L);
	module.SetValue(); check_eq("CKGSeqBackupModuleParam::SetValue", module.m_value, -6780L);
	module.SetMinValue(); check_eq("CKGSeqBackupModuleParam::SetMinValue", module.m_value, 24832L);
	module.SetMaxValue(); check_eq("CKGSeqBackupModuleParam::SetMaxValue", module.m_value, 9154L);
	module.SetKnob(); check_eq("CKGSeqBackupModuleParam::SetKnob", module.m_value, 62L);
	module.SetPolarity(); check_eq("CKGSeqBackupModuleParam::SetPolarity", module.m_value, 159L);
	module.SetValueForModuleControl(); check_eq("CKGSeqBackupModuleParam::SetValueForModuleControl", module.m_value, -25798L);
	module.SetMinValueForModuleControl(); check_eq("CKGSeqBackupModuleParam::SetMinValueForModuleControl", module.m_value, 6070L);
	module.SetMaxValueForModuleControl(); check_eq("CKGSeqBackupModuleParam::SetMaxValueForModuleControl", module.m_value, -9864L);
	module.SetKnobForModuleControl(); check_eq("CKGSeqBackupModuleParam::SetKnobForModuleControl", module.m_value, -12L);
	module.SetPolarityForModuleControl(); check_eq("CKGSeqBackupModuleParam::SetPolarityForModuleControl", module.m_value, 85L);
	module.SetSceneIsLinked(); check_eq("CKGSeqBackupModuleParam::SetSceneIsLinked", module.m_value, 1L);
	module.SetRTCIsLinked(); check_eq("CKGSeqBackupModuleParam::SetRTCIsLinked", module.m_value, 0L);
	module.SetModifiedSw1Value(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw1Value", module.m_value, 1L);
	module.SetModifiedSw2Value(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw2Value", module.m_value, 0L);
	module.SetModifiedSw3Value(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw3Value", module.m_value, 1L);
	module.SetModifiedSw4Value(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw4Value", module.m_value, 0L);
	module.SetModifiedSw5Value(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw5Value", module.m_value, 0L);
	module.SetModifiedSw6Value(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw6Value", module.m_value, 0L);
	module.SetModifiedSw7Value(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw7Value", module.m_value, 1L);
	module.SetModifiedSw8Value(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw8Value", module.m_value, 1L);
	module.SetModifiedSw1Status(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw1Status", module.m_value, 0L);
	module.SetModifiedSw2Status(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw2Status", module.m_value, 0L);
	module.SetModifiedSw3Status(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw3Status", module.m_value, 1L);
	module.SetModifiedSw4Status(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw4Status", module.m_value, 0L);
	module.SetModifiedSw5Status(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw5Status", module.m_value, 0L);
	module.SetModifiedSw6Status(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw6Status", module.m_value, 1L);
	module.SetModifiedSw7Status(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw7Status", module.m_value, 1L);
	module.SetModifiedSw8Status(); check_eq("CKGSeqBackupModuleParam::SetModifiedSw8Status", module.m_value, 0L);
	module.SetModifiedKnob1(); check_eq("CKGSeqBackupModuleParam::SetModifiedKnob1", module.m_value, 3L);
	module.SetModifiedKnob2(); check_eq("CKGSeqBackupModuleParam::SetModifiedKnob2", module.m_value, -94L);
	module.SetModifiedKnob3(); check_eq("CKGSeqBackupModuleParam::SetModifiedKnob3", module.m_value, 65L);
	module.SetModifiedKnob4(); check_eq("CKGSeqBackupModuleParam::SetModifiedKnob4", module.m_value, -32L);
	module.SetModifiedKnob5(); check_eq("CKGSeqBackupModuleParam::SetModifiedKnob5", module.m_value, 127L);
	module.SetModifiedKnob6(); check_eq("CKGSeqBackupModuleParam::SetModifiedKnob6", module.m_value, 30L);
	module.SetModifiedKnob7(); check_eq("CKGSeqBackupModuleParam::SetModifiedKnob7", module.m_value, -67L);
	module.SetModifiedKnob8(); check_eq("CKGSeqBackupModuleParam::SetModifiedKnob8", module.m_value, 92L);
	module.SetSwName(); check_eq("CKGSeqBackupModuleParam::SetSwName", module.m_value, 42822L);
	module.SetKnobName(); check_eq("CKGSeqBackupModuleParam::SetKnobName", module.m_value, 42822L);
	module.SetScene(); check_eq("CKGSeqBackupModuleParam::SetScene", module.m_value, 143L);
	module.SetSw1Value(); check_eq("CKGSeqBackupModuleParam::SetSw1Value", module.m_value, 1L);
	module.SetSw2Value(); check_eq("CKGSeqBackupModuleParam::SetSw2Value", module.m_value, 1L);
	module.SetSw3Value(); check_eq("CKGSeqBackupModuleParam::SetSw3Value", module.m_value, 0L);
	module.SetSw4Value(); check_eq("CKGSeqBackupModuleParam::SetSw4Value", module.m_value, 1L);
	module.SetSw5Value(); check_eq("CKGSeqBackupModuleParam::SetSw5Value", module.m_value, 0L);
	module.SetSw6Value(); check_eq("CKGSeqBackupModuleParam::SetSw6Value", module.m_value, 0L);
	module.SetSw7Value(); check_eq("CKGSeqBackupModuleParam::SetSw7Value", module.m_value, 1L);
	module.SetSw8Value(); check_eq("CKGSeqBackupModuleParam::SetSw8Value", module.m_value, 0L);
	module.SetKnob1Value(); check_eq("CKGSeqBackupModuleParam::SetKnob1Value", module.m_value, 172L);
	module.SetKnob2Value(); check_eq("CKGSeqBackupModuleParam::SetKnob2Value", module.m_value, 13L);
	module.SetKnob3Value(); check_eq("CKGSeqBackupModuleParam::SetKnob3Value", module.m_value, 110L);
	module.SetKnob4Value(); check_eq("CKGSeqBackupModuleParam::SetKnob4Value", module.m_value, 207L);
	module.SetKnob5Value(); check_eq("CKGSeqBackupModuleParam::SetKnob5Value", module.m_value, 48L);
	module.SetKnob6Value(); check_eq("CKGSeqBackupModuleParam::SetKnob6Value", module.m_value, 145L);
	module.SetKnob7Value(); check_eq("CKGSeqBackupModuleParam::SetKnob7Value", module.m_value, 242L);
	module.SetKnob8Value(); check_eq("CKGSeqBackupModuleParam::SetKnob8Value", module.m_value, 83L);
	module.SetQuantizeWindow(); check_eq("CKGSeqBackupModuleParam::SetQuantizeWindow", module.m_value, 4L);
	module.SetLinkToDT(); check_eq("CKGSeqBackupModuleParam::SetLinkToDT", module.m_value, 1L);

	/* Hand-written non-generic-shape methods */
	module.m_index = 3; /* odd -> high-nibble path */
	module.SetLinkedSceneId();
	{
		unsigned char packed = g_dflt[0x2e4 + (3 >> 1)];
		check_eq("CKGSeqBackupModuleParam::SetLinkedSceneId (idx=3, odd)", module.m_value, (packed >> 4) & 0x7);
	}
	module.m_index = 4; /* even -> low-nibble path */
	module.SetLinkedSceneId();
	{
		unsigned char packed = g_dflt[0x2e4 + (4 >> 1)];
		check_eq("CKGSeqBackupModuleParam::SetLinkedSceneId (idx=4, even)", module.m_value, packed & 0x7);
	}

	module.m_index = 3;
	module.SetModCutoff();
	check_eq("CKGSeqBackupModuleParam::SetModCutoff", module.m_value, (long)((g_src[0xd] >> (3 + 4)) & 0x1));

	/*
	 * GetKarmaPerf{Common,Module}ForSeqBackup(): the CSPREngine gate byte
	 * (+0xa) closed -> NULL regardless of what CKGBankManager would
	 * return; gate open + CKGBankManager reports no live record -> NULL;
	 * gate open + a live record -> that record pointer verbatim
	 * (Common) / that record pointer + moduleIndex*0x2e8 (Module, real
	 * confirmed `imul ebx,ebx,0x2e8` stride).
	 */
	printf("\nGetKarmaPerf{Common,Module}ForSeqBackup()\n");
	static unsigned char sprEngine[0x20];
	CSPREngine::ms_poInstance = sprEngine;
	static unsigned char bankMgr[0x97c7e0];
	CKGBankManager::ms_poInstance = bankMgr;
	*(unsigned int *)(bankMgr + 0x97c7d4) = 0x2a; /* arbitrary index echoed to the mock */

	sprEngine[0xa] = 0;
	g_mockCommonPerf = (unsigned char *)0x12345678;
	check_eq("GetKarmaPerfCommonForSeqBackup (gate closed)",
		 (long)common.GetKarmaPerfCommonForSeqBackup(), 0);

	sprEngine[0xa] = 1;
	g_mockCommonPerf = 0;
	check_eq("GetKarmaPerfCommonForSeqBackup (gate open, no live record)",
		 (long)common.GetKarmaPerfCommonForSeqBackup(), 0);

	g_mockCommonPerf = g_src + 0x40;
	check_eq("GetKarmaPerfCommonForSeqBackup (gate open, live record)",
		 (long)common.GetKarmaPerfCommonForSeqBackup(), (long)(g_src + 0x40));

	sprEngine[0xa] = 0;
	g_mockModulePerf = g_src;
	check_eq("GetKarmaPerfModuleForSeqBackup (gate closed)",
		 (long)module.GetKarmaPerfModuleForSeqBackup(5), 0);

	sprEngine[0xa] = 1;
	g_mockModulePerf = 0;
	check_eq("GetKarmaPerfModuleForSeqBackup (gate open, no live record)",
		 (long)module.GetKarmaPerfModuleForSeqBackup(5), 0);

	g_mockModulePerf = g_src;
	check_eq("GetKarmaPerfModuleForSeqBackup (gate open, live record, idx=5)",
		 (long)module.GetKarmaPerfModuleForSeqBackup(5), (long)(g_src + 5 * 0x2e8));

	printf("\n%s\n", g_fail ? "SOME TESTS FAILED" : "all tests passed");
	return g_fail ? 1 : 0;
}
