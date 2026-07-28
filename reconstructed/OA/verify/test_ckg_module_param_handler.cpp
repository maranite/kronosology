// SPDX-License-Identifier: GPL-2.0
/*
 * test_ckg_module_param_handler.cpp  -  KAT for CKGModuleParamMsgHandler
 * (see ../src/engine/ckg_module_param_handler.cpp).
 *
 * Part 1: primary (unconditional) field-write check for all 113 methods
 * reconstructed this batch -- CSPREngine gate held closed (ms_poInstance[0xa]=0)
 * and CKGEngine edit-suppression held closed (ms_poInstance[0xb0]=1) so ONLY the
 * primary m_liveRecord write executes, isolating the field address/width/shift/
 * mask math from the Send()/NotifyAfterEdit()/shadow-write skeleton (that skeleton
 * is separately exercised by Part 2 below for a representative subset). Expected
 * values computed independently here (Python-derived constants), not by re-using
 * the source file's own C expressions.
 * idx (msg.m_index) fixed at 3 for every ctx-indexed method. m_liveRecord is
 * pre-filled with a distinguishable non-zero pattern so bitfield read-modify-write
 * neighbor-bit preservation is genuinely exercised, not just the target bits.
 */

#include <cstdio>
#include <cstring>
#include "oa_ckg_module_param_msg_handler.h"

/* host-only mock storage/bodies for every out-of-scope singleton/dependency */
/* CKGEngine::ms_poInstance/ms_poKGParamEdit, CKGUIMsgProcessor::ms_poInstance,
 * and CSPRMIDIMsgProcessor::ms_poSysExPlayBuf are all defined in
 * ckg_module_param_handler.cpp itself (this test links that file) --
 * only CSPREngine and CKGBankManager's own storage (defined in
 * karma_seq_backup.cpp / sk_stg_gate.cpp respectively, deliberately NOT
 * linked here) needs a host-side stand-in. */
unsigned char *CSPREngine::ms_poInstance;
unsigned char *CKGBankManager::ms_poInstance;
static int g_sendCalls;
static int g_notifyCalls;
static int g_sysexGetValueCalls;
static int g_sysexGetValueReturn;
void CKGParamEdit::SendSolo(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendCCSource(unsigned char, unsigned char, char) { g_sendCalls++; }
void CKGParamEdit::SendCCValue(unsigned char, unsigned char, char) { g_sendCalls++; }
void CKGParamEdit::SendClkAdvMode(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendClkAdvSize(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendClkAdvTrig(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendClkAdvVelSense(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendCutoffPercent(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendDelayMode(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendDelayTime(unsigned char, short) { g_sendCalls++; }
void CKGParamEdit::SendEnvLatchMode(unsigned char, unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendEnvTrigMode(unsigned char, unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendForceRange(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendForceRangeWrap(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendFreezeLoop(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendFreezeLoopRetrig(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendGEDestKnob(unsigned char, unsigned char, char) { g_sendCalls++; }
void CKGParamEdit::SendGEDestKnobForModuleControl(unsigned char, unsigned char, char) { g_sendCalls++; }
void CKGParamEdit::SendGEMaxValue(unsigned char, unsigned char, short) { g_sendCalls++; }
void CKGParamEdit::SendGEMaxValueForModuleControl(unsigned char, unsigned char, short) { g_sendCalls++; }
void CKGParamEdit::SendGEMinValue(unsigned char, unsigned char, short) { g_sendCalls++; }
void CKGParamEdit::SendGEMinValueForModuleControl(unsigned char, unsigned char, short) { g_sendCalls++; }
void CKGParamEdit::SendGEPolarity(unsigned char, unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendGEPolarityForModuleControl(unsigned char, unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendGEValue(unsigned char, unsigned char, short) { g_sendCalls++; }
void CKGParamEdit::SendGEValueForModuleControl(unsigned char, unsigned char, short) { g_sendCalls++; }
void CKGParamEdit::SendInputCh(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendKbdInTranspose(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendKbdOutTranspose(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendKeyBottom(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendKeyTop(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendKeyboardIn(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendKeyboardOut(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendLinkToDT(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendModCutOff(unsigned char, unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendNoteLatchMode(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendNoteMap(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendNoteMapChdTrack(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendNoteMapKbdTrack(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendNoteMapOnMode(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendNoteMapTranspose(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendNoteTrigMode(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendOutputCh(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendQuantize(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendQuantizeWindow(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendRTCIsLinked(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendRootPosition(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendRun(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendRxAfter(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendRxBend(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendRxDamper(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendRxJSYM(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendRxJSYP(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendRxOther(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendRxRibbon(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendSceneIsLinked(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendSeedCluster(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendSeedDrum(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendSeedDuration(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendSeedNote(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendSeedPan(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendSeedRhythm(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendSeedVelocity(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendSeedWaveform(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendStartSeed(unsigned char, unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendTimbreThru(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendTranspose(unsigned char, char) { g_sendCalls++; }
void CKGParamEdit::SendTrigModMode(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendTxBend(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendTxCCA(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendTxCCB(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendTxEnv1(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendTxEnv2(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendTxEnv3(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendTxNote(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendTxWaveform(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendUpdateOnRelease(unsigned char, unsigned char) { g_sendCalls++; }
void CKGParamEdit::SendUseGChAlso(unsigned char, unsigned char) { g_sendCalls++; }
unsigned int CKGParamEdit::GetRTParmBufferSelectId(int) { return 0; }
void CKGUIMsgProcessor::NotifyAfterEdit() { g_notifyCalls++; }
char CSPRSysExBufManager::GetValue(int,int,int,int,int,int,int,long*) { g_sysexGetValueCalls++; return (char)g_sysexGetValueReturn; }
unsigned char *CKGBankManager::GetSeqKarmaPerfCommon(unsigned int) { return 0; }
unsigned char *CKGBankManager::GetSeqKarmaPerfModule(unsigned int idx) { return (unsigned char *)(unsigned long)(0x1000 + idx); }
unsigned char *CKGBankManager::GetSeqDefaultKarmaPerfCommon() { return 0; }
unsigned char *CKGBankManager::GetCombiKarmaPerfModule(eSTGCombiBankId, unsigned int idx) { return (unsigned char *)(unsigned long)(0x2000 + idx); }
unsigned char *CKGBankManager::GetProgKarmaPerfModule(eSTGProgramBankId, unsigned int idx) { return (unsigned char *)(unsigned long)(0x3000 + idx); }

/* batch 2 (2026-07-28): Shape C/D Knob/Sw value group + SetScene/
 * SetLinkedSceneId mocks. */
static int g_selectIdArg = -1, g_indexArg = -1, g_boolArg = -1, g_lastValueArg = 0;
void CKGParamEdit::SendKnob(int selectId, int index, int, int value, bool) { g_sendCalls++; g_selectIdArg = selectId; g_indexArg = index; g_lastValueArg = value; }
void CKGParamEdit::SendAssignableSwitch(int selectId, int index, int, bool onOff, bool) { g_sendCalls++; g_selectIdArg = selectId; g_indexArg = index; g_boolArg = onOff; }
void CKGParamEdit::SendScene(int selectId, unsigned char value, bool) { g_sendCalls++; g_selectIdArg = selectId; g_lastValueArg = value; }
void CKGParamEdit::SendLinkedSceneID(unsigned char deviceIndex, unsigned char index, unsigned char value) { g_sendCalls++; g_selectIdArg = deviceIndex; g_indexArg = index; g_lastValueArg = value; }
static int g_notifySliderCalls;
void SKSTGGate_NotifyKarmaSliderPosition(int) { g_notifySliderCalls++; }

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld\n", label, got); return; }
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x800
static unsigned char g_live[BUFSZ];

int main(void)
{
	printf("CKGModuleParamMsgHandler known-answer test\n");
	printf("========================================================================\n");

	CSPREngine::ms_poInstance = new unsigned char[0x10](); /* [0xa]=0: gate closed */
	CKGEngine::ms_poInstance = new unsigned char[0x100](); CKGEngine::ms_poInstance[0xb0] = 1; /* suppressed */
	CKGUIMsgProcessor::ms_poInstance = new unsigned char[0x100]();
	CKGBankManager::ms_poInstance = new unsigned char[0x1000000]();
	/* SetScene's own curSceneIdx read is a genuine double indirection
	 * (*(byte*)(*(byte**)CKGBankManager::ms_poInstance + 0x135)) -- give
	 * the mock a real inner buffer at offset 0 so that dereference doesn't
	 * chase a null pointer. */
	static unsigned char g_bankInner[0x200];
	*(unsigned char **)CKGBankManager::ms_poInstance = g_bankInner;

	CKGModuleParamMsgHandler h;
	memset(&h, 0, sizeof(h));

	CKGModuleParamMsg msg;
	memset(&msg, 0, sizeof(msg));
	msg.m_index = 3;
	msg.m_deviceIndex = 2;

	printf("--- Part 1: primary field write (gates closed) ---\n");
	for (unsigned int i = 0; i < BUFSZ; i++)
		g_live[i] = (unsigned char)(i*0x53 + 0x81);
	h.m_liveRecord = g_live;
	h.m_defaultRecordA = 0;
	h.m_defaultRecordB = 0;
	msg.m_value = 305441741;
	h.SetModifiedKnob1(&msg);
	check_eq("SetModifiedKnob1", (signed char)g_live[0x2b4], (signed char)(unsigned char)0x1234abcd);
	msg.m_value = 305441741;
	h.SetModifiedKnob2(&msg);
	check_eq("SetModifiedKnob2", (signed char)g_live[0x2b5], (signed char)(unsigned char)0x1234abcd);
	msg.m_value = 305441741;
	h.SetModifiedKnob3(&msg);
	check_eq("SetModifiedKnob3", (signed char)g_live[0x2b6], (signed char)(unsigned char)0x1234abcd);
	msg.m_value = 305441741;
	h.SetModifiedKnob4(&msg);
	check_eq("SetModifiedKnob4", (signed char)g_live[0x2b7], (signed char)(unsigned char)0x1234abcd);
	msg.m_value = 305441741;
	h.SetModifiedKnob5(&msg);
	check_eq("SetModifiedKnob5", (signed char)g_live[0x2b8], (signed char)(unsigned char)0x1234abcd);
	msg.m_value = 305441741;
	h.SetModifiedKnob6(&msg);
	check_eq("SetModifiedKnob6", (signed char)g_live[0x2b9], (signed char)(unsigned char)0x1234abcd);
	msg.m_value = 305441741;
	h.SetModifiedKnob7(&msg);
	check_eq("SetModifiedKnob7", (signed char)g_live[0x2ba], (signed char)(unsigned char)0x1234abcd);
	msg.m_value = 305441741;
	h.SetModifiedKnob8(&msg);
	check_eq("SetModifiedKnob8", (signed char)g_live[0x2bb], (signed char)(unsigned char)0x1234abcd);
	{ unsigned char pre = g_live[0x2b2]; msg.m_value = 1; h.SetModifiedSw1Value(&msg);
	  check_eq("SetModifiedSw1Value bit set", (g_live[0x2b2] >> 0) & 1, 1);
	  check_eq("SetModifiedSw1Value neighbors preserved", g_live[0x2b2] | (1<<0), pre | (1<<0)); }
	{ unsigned char pre = g_live[0x2b2]; msg.m_value = 1; h.SetModifiedSw2Value(&msg);
	  check_eq("SetModifiedSw2Value bit set", (g_live[0x2b2] >> 1) & 1, 1);
	  check_eq("SetModifiedSw2Value neighbors preserved", g_live[0x2b2] | (1<<1), pre | (1<<1)); }
	{ unsigned char pre = g_live[0x2b2]; msg.m_value = 1; h.SetModifiedSw3Value(&msg);
	  check_eq("SetModifiedSw3Value bit set", (g_live[0x2b2] >> 2) & 1, 1);
	  check_eq("SetModifiedSw3Value neighbors preserved", g_live[0x2b2] | (1<<2), pre | (1<<2)); }
	{ unsigned char pre = g_live[0x2b2]; msg.m_value = 1; h.SetModifiedSw4Value(&msg);
	  check_eq("SetModifiedSw4Value bit set", (g_live[0x2b2] >> 3) & 1, 1);
	  check_eq("SetModifiedSw4Value neighbors preserved", g_live[0x2b2] | (1<<3), pre | (1<<3)); }
	{ unsigned char pre = g_live[0x2b2]; msg.m_value = 1; h.SetModifiedSw5Value(&msg);
	  check_eq("SetModifiedSw5Value bit set", (g_live[0x2b2] >> 4) & 1, 1);
	  check_eq("SetModifiedSw5Value neighbors preserved", g_live[0x2b2] | (1<<4), pre | (1<<4)); }
	{ unsigned char pre = g_live[0x2b2]; msg.m_value = 1; h.SetModifiedSw6Value(&msg);
	  check_eq("SetModifiedSw6Value bit set", (g_live[0x2b2] >> 5) & 1, 1);
	  check_eq("SetModifiedSw6Value neighbors preserved", g_live[0x2b2] | (1<<5), pre | (1<<5)); }
	{ unsigned char pre = g_live[0x2b2]; msg.m_value = 1; h.SetModifiedSw7Value(&msg);
	  check_eq("SetModifiedSw7Value bit set", (g_live[0x2b2] >> 6) & 1, 1);
	  check_eq("SetModifiedSw7Value neighbors preserved", g_live[0x2b2] | (1<<6), pre | (1<<6)); }
	{ unsigned char pre = g_live[0x2b2]; msg.m_value = 1; h.SetModifiedSw8Value(&msg);
	  check_eq("SetModifiedSw8Value bit set", (g_live[0x2b2] >> 7) & 1, 1);
	  check_eq("SetModifiedSw8Value neighbors preserved", g_live[0x2b2] | (1<<7), pre | (1<<7)); }
	{ unsigned char pre = g_live[0x2b3]; msg.m_value = 1; h.SetModifiedSw1Status(&msg);
	  check_eq("SetModifiedSw1Status bit set", (g_live[0x2b3] >> 0) & 1, 1);
	  check_eq("SetModifiedSw1Status neighbors preserved", g_live[0x2b3] | (1<<0), pre | (1<<0)); }
	{ unsigned char pre = g_live[0x2b3]; msg.m_value = 1; h.SetModifiedSw2Status(&msg);
	  check_eq("SetModifiedSw2Status bit set", (g_live[0x2b3] >> 1) & 1, 1);
	  check_eq("SetModifiedSw2Status neighbors preserved", g_live[0x2b3] | (1<<1), pre | (1<<1)); }
	{ unsigned char pre = g_live[0x2b3]; msg.m_value = 1; h.SetModifiedSw3Status(&msg);
	  check_eq("SetModifiedSw3Status bit set", (g_live[0x2b3] >> 2) & 1, 1);
	  check_eq("SetModifiedSw3Status neighbors preserved", g_live[0x2b3] | (1<<2), pre | (1<<2)); }
	{ unsigned char pre = g_live[0x2b3]; msg.m_value = 1; h.SetModifiedSw4Status(&msg);
	  check_eq("SetModifiedSw4Status bit set", (g_live[0x2b3] >> 3) & 1, 1);
	  check_eq("SetModifiedSw4Status neighbors preserved", g_live[0x2b3] | (1<<3), pre | (1<<3)); }
	{ unsigned char pre = g_live[0x2b3]; msg.m_value = 1; h.SetModifiedSw5Status(&msg);
	  check_eq("SetModifiedSw5Status bit set", (g_live[0x2b3] >> 4) & 1, 1);
	  check_eq("SetModifiedSw5Status neighbors preserved", g_live[0x2b3] | (1<<4), pre | (1<<4)); }
	{ unsigned char pre = g_live[0x2b3]; msg.m_value = 1; h.SetModifiedSw6Status(&msg);
	  check_eq("SetModifiedSw6Status bit set", (g_live[0x2b3] >> 5) & 1, 1);
	  check_eq("SetModifiedSw6Status neighbors preserved", g_live[0x2b3] | (1<<5), pre | (1<<5)); }
	{ unsigned char pre = g_live[0x2b3]; msg.m_value = 1; h.SetModifiedSw7Status(&msg);
	  check_eq("SetModifiedSw7Status bit set", (g_live[0x2b3] >> 6) & 1, 1);
	  check_eq("SetModifiedSw7Status neighbors preserved", g_live[0x2b3] | (1<<6), pre | (1<<6)); }
	{ unsigned char pre = g_live[0x2b3]; msg.m_value = 1; h.SetModifiedSw8Status(&msg);
	  check_eq("SetModifiedSw8Status bit set", (g_live[0x2b3] >> 7) & 1, 1);
	  check_eq("SetModifiedSw8Status neighbors preserved", g_live[0x2b3] | (1<<7), pre | (1<<7)); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x8];
	  h.SetClkAdvCtrig(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0xf << 0)) | ((unsigned char)(((unsigned int)305441741) & 0xf) << 0));
	  check_eq("SetClkAdvCtrig", g_live[0x8], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x7];
	  h.SetClkAdvMode(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x3 << 6)) | ((unsigned char)(((unsigned int)305441741) & 0x3) << 6));
	  check_eq("SetClkAdvMode", g_live[0x7], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x8];
	  h.SetClkAdvSize(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0xf << 4)) | ((unsigned char)(((unsigned int)305441741) & 0xf) << 4));
	  check_eq("SetClkAdvSize", g_live[0x8], want); }
	msg.m_value = 305441741;
	h.SetClkAdvVSence(&msg);
	check_eq("SetClkAdvVSence", (unsigned char)*(unsigned char*)(g_live+0x9), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x7];
	  h.SetCollapse(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x7 << 3)) | ((unsigned char)(((unsigned int)305441741) & 0x7) << 3));
	  check_eq("SetCollapse", g_live[0x7], want); }
	msg.m_value = 305441741;
	h.SetDelayMode(&msg);
	check_eq("SetDelayMode", (unsigned char)*(unsigned char*)(g_live+0xc), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetDelayTime(&msg);
	check_eq("SetDelayTime", (short)*(short*)(g_live+0xa), (short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x10];
	  h.SetEnv1Latch(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0xf << 4)) | ((unsigned char)(((unsigned int)305441741) & 0xf) << 4));
	  check_eq("SetEnv1Latch", g_live[0x10], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x10];
	  h.SetEnv1Trig(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0xf << 0)) | ((unsigned char)(((unsigned int)305441741) & 0xf) << 0));
	  check_eq("SetEnv1Trig", g_live[0x10], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x11];
	  h.SetEnv2Latch(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0xf << 4)) | ((unsigned char)(((unsigned int)305441741) & 0xf) << 4));
	  check_eq("SetEnv2Latch", g_live[0x11], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x11];
	  h.SetEnv2Trig(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0xf << 0)) | ((unsigned char)(((unsigned int)305441741) & 0xf) << 0));
	  check_eq("SetEnv2Trig", g_live[0x11], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x12];
	  h.SetEnv3Latch(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0xf << 4)) | ((unsigned char)(((unsigned int)305441741) & 0xf) << 4));
	  check_eq("SetEnv3Latch", g_live[0x12], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x12];
	  h.SetEnv3Trig(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0xf << 0)) | ((unsigned char)(((unsigned int)305441741) & 0xf) << 0));
	  check_eq("SetEnv3Trig", g_live[0x12], want); }
	msg.m_value = 305441741;
	h.SetForceRangeWrap(&msg);
	check_eq("SetForceRangeWrap", (unsigned char)*(unsigned char*)(g_live+0x192), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x19];
	  h.SetFreezeLoop(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x3f << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x3f) << 0));
	  check_eq("SetFreezeLoop", g_live[0x19], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x19];
	  h.SetFreezeRetrig(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 7)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 7));
	  check_eq("SetFreezeRetrig", g_live[0x19], want); }
	msg.m_value = 305441741;
	h.SetGE(&msg);
	check_eq("SetGE", (short)*(short*)(g_live+0x0), (short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetGenCC(&msg);
	check_eq("SetGenCC", (signed char)*(signed char*)(g_live+0x124), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetGenCCValue(&msg);
	check_eq("SetGenCCValue", (unsigned char)*(unsigned char*)(g_live+0x125), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x2];
	  h.SetInputCh(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1f << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x1f) << 0));
	  check_eq("SetInputCh", g_live[0x2], want); }
	msg.m_value = 305441741;
	h.SetKIZoneTrans(&msg);
	check_eq("SetKIZoneTrans", (signed char)*(signed char*)(g_live+0x17), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetKOZoneTrans(&msg);
	check_eq("SetKOZoneTrans", (signed char)*(signed char*)(g_live+0x18), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x126];
	  h.SetKbdInZone(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 3)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 3));
	  check_eq("SetKbdInZone", g_live[0x126], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x126];
	  h.SetKbdOutZone(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 4)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 4));
	  check_eq("SetKbdOutZone", g_live[0x126], want); }
	msg.m_value = 305441741;
	h.SetKeyBottom(&msg);
	check_eq("SetKeyBottom", (unsigned char)*(unsigned char*)(g_live+0x6), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetKeyTop(&msg);
	check_eq("SetKeyTop", (unsigned char)*(unsigned char*)(g_live+0x5), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetKnob(&msg);
	check_eq("SetKnob", (signed char)*(signed char*)(g_live+0x36), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetKnobForModuleControl(&msg);
	check_eq("SetKnobForModuleControl", (signed char)*(signed char*)(g_live+0x1ac), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetKnobName(&msg);
	check_eq("SetKnobName", (unsigned short)*(unsigned short*)(g_live+0x13e), (unsigned short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x126];
	  h.SetLinkToDT(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 7)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 7));
	  check_eq("SetLinkToDT", g_live[0x126], want); }
	msg.m_value = 305441741;
	h.SetMaxValue(&msg);
	check_eq("SetMaxValue", (short)*(short*)(g_live+0x3a), (short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetMaxValueForModuleControl(&msg);
	check_eq("SetMaxValueForModuleControl", (short)*(short*)(g_live+0x1b0), (short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetMinValue(&msg);
	check_eq("SetMinValue", (short)*(short*)(g_live+0x38), (short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetMinValueForModuleControl(&msg);
	check_eq("SetMinValueForModuleControl", (short)*(short*)(g_live+0x1ae), (short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0xd];
	  h.SetModCutoff(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 0));
	  check_eq("SetModCutoff", g_live[0xd], want); }
	msg.m_value = 305441741;
	h.SetModPercent(&msg);
	check_eq("SetModPercent", (unsigned char)*(unsigned char*)(g_live+0xe), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0xf];
	  h.SetNoteLatch(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 4)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 4));
	  check_eq("SetNoteLatch", g_live[0xf], want); }
	msg.m_value = 305441741;
	h.SetNoteMap(&msg);
	check_eq("SetNoteMap", (unsigned char)*(unsigned char*)(g_live+0x190), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x191];
	  h.SetNoteMapChdTrack(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 6)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 6));
	  check_eq("SetNoteMapChdTrack", g_live[0x191], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x191];
	  h.SetNoteMapKbdTrack(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 7)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 7));
	  check_eq("SetNoteMapKbdTrack", g_live[0x191], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x191];
	  h.SetNoteMapOnMode(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x3 << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x3) << 0));
	  check_eq("SetNoteMapOnMode", g_live[0x191], want); }
	msg.m_value = 305441741;
	h.SetNoteMapTranspose(&msg);
	check_eq("SetNoteMapTranspose", (signed char)*(signed char*)(g_live+0x193), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0xf];
	  h.SetNoteTrig(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0xf << 0)) | ((unsigned char)(((unsigned int)305441741) & 0xf) << 0));
	  check_eq("SetNoteTrig", g_live[0xf], want); }
	msg.m_value = 305441741;
	h.SetOutputCh(&msg);
	check_eq("SetOutputCh", (unsigned char)*(unsigned char*)(g_live+0x3), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetPolarity(&msg);
	check_eq("SetPolarity", (unsigned char)*(unsigned char*)(g_live+0x37), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetPolarityForModuleControl(&msg);
	check_eq("SetPolarityForModuleControl", (unsigned char)*(unsigned char*)(g_live+0x1ad), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x126];
	  h.SetQuantize(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 0));
	  check_eq("SetQuantize", g_live[0x126], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0xf];
	  h.SetQuantizeWindow(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x7 << 5)) | ((unsigned char)(((unsigned int)305441741) & 0x7) << 5));
	  check_eq("SetQuantizeWindow", g_live[0xf], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x2e4];
	  h.SetRTCIsLinked(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 7)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 7));
	  check_eq("SetRTCIsLinked", g_live[0x2e4], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x15];
	  h.SetRndCluster(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x3 << 6)) | ((unsigned char)(((unsigned int)305441741) & 0x3) << 6));
	  check_eq("SetRndCluster", g_live[0x15], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x16];
	  h.SetRndDrum(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x3 << 4)) | ((unsigned char)(((unsigned int)305441741) & 0x3) << 4));
	  check_eq("SetRndDrum", g_live[0x16], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x15];
	  h.SetRndDuration(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x3 << 2)) | ((unsigned char)(((unsigned int)305441741) & 0x3) << 2));
	  check_eq("SetRndDuration", g_live[0x15], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x15];
	  h.SetRndNote(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x3 << 4)) | ((unsigned char)(((unsigned int)305441741) & 0x3) << 4));
	  check_eq("SetRndNote", g_live[0x15], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x16];
	  h.SetRndPan(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x3 << 2)) | ((unsigned char)(((unsigned int)305441741) & 0x3) << 2));
	  check_eq("SetRndPan", g_live[0x16], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x15];
	  h.SetRndRhythm(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x3 << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x3) << 0));
	  check_eq("SetRndRhythm", g_live[0x15], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x16];
	  h.SetRndVelocity(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x3 << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x3) << 0));
	  check_eq("SetRndVelocity", g_live[0x16], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x16];
	  h.SetRndWaveform(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x3 << 6)) | ((unsigned char)(((unsigned int)305441741) & 0x3) << 6));
	  check_eq("SetRndWaveform", g_live[0x16], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x7];
	  h.SetRootPosition(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 0));
	  check_eq("SetRootPosition", g_live[0x7], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x126];
	  h.SetRun(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 5)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 5));
	  check_eq("SetRun", g_live[0x126], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x14];
	  h.SetRxAfter(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 1)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 1));
	  check_eq("SetRxAfter", g_live[0x14], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x14];
	  h.SetRxBend(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 0));
	  check_eq("SetRxBend", g_live[0x14], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x14];
	  h.SetRxDamper(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 2)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 2));
	  check_eq("SetRxDamper", g_live[0x14], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x14];
	  h.SetRxJSYM(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 4)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 4));
	  check_eq("SetRxJSYM", g_live[0x14], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x14];
	  h.SetRxJSYP(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 3)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 3));
	  check_eq("SetRxJSYP", g_live[0x14], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x14];
	  h.SetRxOther(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 5)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 5));
	  check_eq("SetRxOther", g_live[0x14], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x14];
	  h.SetRxRibbon(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 6)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 6));
	  check_eq("SetRxRibbon", g_live[0x14], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x2e4];
	  h.SetSceneIsLinked(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 3)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 3));
	  check_eq("SetSceneIsLinked", g_live[0x2e4], want); }
	msg.m_value = 305441741;
	h.SetSeed(&msg);
	check_eq("SetSeed", (unsigned char)*(unsigned char*)(g_live+0x1d), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetSwName(&msg);
	check_eq("SetSwName", (unsigned short)*(unsigned short*)(g_live+0x13e), (unsigned short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x126];
	  h.SetTZoneBypass(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 1)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 1));
	  check_eq("SetTZoneBypass", g_live[0x126], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x126];
	  h.SetThru(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 2)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 2));
	  check_eq("SetThru", g_live[0x126], want); }
	msg.m_value = 305441741;
	h.SetTranspose(&msg);
	check_eq("SetTranspose", (signed char)*(signed char*)(g_live+0x4), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0xd];
	  h.SetTrigModule(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0xf << 0)) | ((unsigned char)(((unsigned int)305441741) & 0xf) << 0));
	  check_eq("SetTrigModule", g_live[0xd], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x13];
	  h.SetTxBend(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 0));
	  check_eq("SetTxBend", g_live[0x13], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x13];
	  h.SetTxCCA(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 1)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 1));
	  check_eq("SetTxCCA", g_live[0x13], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x13];
	  h.SetTxCCB(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 2)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 2));
	  check_eq("SetTxCCB", g_live[0x13], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x13];
	  h.SetTxEnv1(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 3)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 3));
	  check_eq("SetTxEnv1", g_live[0x13], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x13];
	  h.SetTxEnv2(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 4)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 4));
	  check_eq("SetTxEnv2", g_live[0x13], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x13];
	  h.SetTxEnv3(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 5)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 5));
	  check_eq("SetTxEnv3", g_live[0x13], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x13];
	  h.SetTxNote(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 6)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 6));
	  check_eq("SetTxNote", g_live[0x13], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x13];
	  h.SetTxWaveform(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 7)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 7));
	  check_eq("SetTxWaveform", g_live[0x13], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x2];
	  h.SetUseGChAlso(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 7)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 7));
	  check_eq("SetUseGChAlso", g_live[0x2], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x126];
	  h.SetUseNoteOffs(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 6)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 6));
	  check_eq("SetUseNoteOffs", g_live[0x126], want); }
	msg.m_value = 305441741;
	h.SetValue(&msg);
	check_eq("SetValue", (short)*(short*)(g_live+0x3c), (short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetValueForModuleControl(&msg);
	check_eq("SetValueForModuleControl", (short)*(short*)(g_live+0x1b2), (short)(unsigned short)((unsigned int)305441741));

	/* batch 2 (2026-07-28): Shape C/D Knob/Sw value group primary writes,
	 * idx still fixed at 3. Expected bytes computed independently in
	 * Python (value&0xff=0xcd; RMW fields computed against g_live's own
	 * deterministic pre-fill formula), not by re-using the source's own
	 * C expressions. */
	msg.m_value = 305441741;
	h.SetKnob1Value(&msg); check_eq("SetKnob1Value", g_live[0x164], 0xcd);
	h.SetKnob2Value(&msg); check_eq("SetKnob2Value", g_live[0x165], 0xcd);
	h.SetKnob3Value(&msg); check_eq("SetKnob3Value", g_live[0x166], 0xcd);
	h.SetKnob4Value(&msg); check_eq("SetKnob4Value", g_live[0x167], 0xcd);
	h.SetKnob5Value(&msg); check_eq("SetKnob5Value", g_live[0x168], 0xcd);
	h.SetKnob6Value(&msg); check_eq("SetKnob6Value", g_live[0x169], 0xcd);
	h.SetKnob7Value(&msg); check_eq("SetKnob7Value", g_live[0x16a], 0xcd);
	h.SetKnob8Value(&msg); check_eq("SetKnob8Value", g_live[0x16b], 0xcd);
	/* Sw base byte at 0x163 (idx*9+0x148), pre-fill value 0x9a */
	h.SetSw1Value(&msg); check_eq("SetSw1Value", g_live[0x163], 0xdf); g_live[0x163] = 0x9a;
	h.SetSw2Value(&msg); check_eq("SetSw2Value", g_live[0x163], 0x9a); g_live[0x163] = 0x9a;
	h.SetSw3Value(&msg); check_eq("SetSw3Value", g_live[0x163], 0xbe); g_live[0x163] = 0x9a;
	h.SetSw4Value(&msg); check_eq("SetSw4Value", g_live[0x163], 0xfa); g_live[0x163] = 0x9a;
	h.SetSw5Value(&msg); check_eq("SetSw5Value", g_live[0x163], 0xda); g_live[0x163] = 0x9a;
	h.SetSw6Value(&msg); check_eq("SetSw6Value", g_live[0x163], 0xba); g_live[0x163] = 0x9a;
	h.SetSw7Value(&msg); check_eq("SetSw7Value", g_live[0x163], 0xda); g_live[0x163] = 0x9a;
	h.SetSw8Value(&msg); check_eq("SetSw8Value", g_live[0x163], 0x9a); g_live[0x163] = 0x9a;

	/* SetScene: primary 0x127 write is unconditional even suppressed;
	 * the LinkedSceneId-style nibble sync is gated on suppression (still
	 * suppressed here, Part 1), so it must NOT fire */
	g_live[0x2e5] = 0xc0;	/* re-seed to the deterministic pre-fill value */
	*(unsigned char *)(*(unsigned char **)CKGBankManager::ms_poInstance + 0x135) = 3; /* curSceneIdx=3, matches idx used for the pre-fill math above */
	h.SetScene(&msg);
	check_eq("SetScene primary 0x127", g_live[0x127], 0xcd);
	check_eq("SetScene nibble untouched (suppressed)", g_live[0x2e5], 0xc0);

	/* SetLinkedSceneId: inverted -- nibble write unconditional even
	 * suppressed, the 0x127 mirror is gated on suppression and must NOT
	 * fire here */
	g_live[0x127] = 0x26;	/* re-seed to the deterministic pre-fill value */
	h.SetLinkedSceneId(&msg);
	check_eq("SetLinkedSceneId nibble (unconditional)", g_live[0x2e5], 0xd0);
	check_eq("SetLinkedSceneId 0x127 untouched (suppressed)", g_live[0x127], 0x26);

	printf("--- Part 2: gate + shadow-write + Send/Notify skeleton, representative subset ---\n");

	/* re-open the CSPREngine gate and give the handler both shadow-record
	 * pointers + a UI mode OUTSIDE the {8,9,10} range (and != 4), so
	 * ShouldAttemptSysExShadowWrite() takes the fallback branch. Mode
	 * value flipped 2026-07-28 after a ground-truth re-check (3
	 * independent disassemblies: SetValue/SetKnob1Value/SetGenCC) found
	 * the shadow-attempt is taken when mode is NOT in {8,9,10}, the
	 * opposite of this test's original mode=9 -- see the bug-fix comment
	 * on ShouldAttemptSysExShadowWrite()'s own body. */
	CSPREngine::ms_poInstance[0xa] = 1;
	static unsigned char defaultA[BUFSZ], defaultB[BUFSZ];
	memset(defaultA, 0x55, BUFSZ);
	memset(defaultB, 0x66, BUFSZ);
	h.m_defaultRecordA = defaultA;
	h.m_defaultRecordB = defaultB;
	h.m_moduleIndex = 7;
	*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) = 20;	/* NOT in {8,9,10}, != 4 */
	CKGEngine::ms_poInstance[0xb0] = 0;			/* edits NOT suppressed */

	/* sub-case A: SysEx lookup MISSES (returns 0) -> shadow write happens,
	 * flag74 gets set, single-shadow field (SetOutputCh) */
	g_sysexGetValueReturn = 0;
	g_sysexGetValueCalls = 0;
	g_sendCalls = 0;
	g_notifyCalls = 0;
	CKGUIMsgProcessor::ms_poInstance[0x74] = 0;
	msg.m_value = 0x42;
	h.SetOutputCh(&msg);
	check_eq("SetOutputCh sysex GetValue called", g_sysexGetValueCalls, 1);
	check_eq("SetOutputCh flag74 set on miss", CKGUIMsgProcessor::ms_poInstance[0x74], 1);
	check_eq("SetOutputCh shadowA written on miss", defaultA[0x3], 0x42);
	check_eq("SetOutputCh Send still called (unsuppressed)", g_sendCalls, 1);
	check_eq("SetOutputCh Notify still called (unsuppressed)", g_notifyCalls, 1);

	/* sub-case B: SysEx lookup HITS (returns nonzero) -> shadow write
	 * skipped, but Send/Notify still fire (steps 2 and 3 are independent,
	 * not an if/else -- see header comment) */
	g_sysexGetValueReturn = 1;
	CKGUIMsgProcessor::ms_poInstance[0x74] = 0;
	defaultA[0x3] = 0xAA;
	g_sendCalls = 0;
	g_notifyCalls = 0;
	msg.m_value = 0x11;
	h.SetOutputCh(&msg);
	check_eq("SetOutputCh shadowA untouched on hit", defaultA[0x3], 0xAA);
	check_eq("SetOutputCh flag74 untouched on hit", CKGUIMsgProcessor::ms_poInstance[0x74], 0);
	check_eq("SetOutputCh Send still called on hit", g_sendCalls, 1);
	check_eq("SetOutputCh Notify still called on hit", g_notifyCalls, 1);

	/* sub-case C: dual-shadow field (SetKnob, base=default+ctx-indexed) --
	 * both defaultA AND defaultB must be written on a miss */
	g_sysexGetValueReturn = 0;
	msg.m_value = 0x37;
	h.SetKnob(&msg);
	unsigned int knobAddr = 3*8 + 0x1e;
	check_eq("SetKnob shadowA written", defaultA[knobAddr], (signed char)0x37);
	check_eq("SetKnob shadowB written", defaultB[knobAddr], (signed char)0x37);

	/* sub-case D: single-shadow ctx-indexed field (SetGenCC, base=source)
	 * -- defaultB must stay UNTOUCHED */
	memset(defaultB, 0x66, BUFSZ);
	msg.m_value = 0x21;
	h.SetGenCC(&msg);
	unsigned int gencAddr = 3*2 + 0x11e;
	check_eq("SetGenCC shadowB NOT touched (single-shadow field)", defaultB[gencAddr], 0x66);

	/* sub-case E: SetSolo -- no CSPREngine/shadow gate at all, always
	 * calls Send+Notify purely off the CKGEngine suppression flag */
	g_sendCalls = 0;
	g_notifyCalls = 0;
	CKGEngine::ms_poInstance[0xb0] = 1;	/* suppressed */
	h.SetSolo(&msg);
	check_eq("SetSolo suppressed: no Send", g_sendCalls, 0);
	check_eq("SetSolo suppressed: no Notify", g_notifyCalls, 0);
	CKGEngine::ms_poInstance[0xb0] = 0;	/* unsuppressed */
	h.SetSolo(&msg);
	check_eq("SetSolo unsuppressed: Send called", g_sendCalls, 1);
	check_eq("SetSolo unsuppressed: Notify called", g_notifyCalls, 1);

	/* sub-case F: ShouldStoreToBackup -- true only when the gate is open
	 * AND the SysEx lookup misses */
	g_sysexGetValueReturn = 0;
	check_eq("ShouldStoreToBackup true on miss", h.ShouldStoreToBackup(&msg), 1);
	g_sysexGetValueReturn = 1;
	check_eq("ShouldStoreToBackup false on hit", h.ShouldStoreToBackup(&msg), 0);
	CSPREngine::ms_poInstance[0xa] = 0;
	check_eq("ShouldStoreToBackup false, gate closed", h.ShouldStoreToBackup(&msg), 0);
	CSPREngine::ms_poInstance[0xa] = 1;

	/* sub-case G (batch 2): SetKnob1Value -- RTParm-indirected Send +
	 * dual-shadow + conditional SKSTGGate_NotifyKarmaSliderPosition()
	 * tail call gated on UI mode != 1 */
	g_sysexGetValueReturn = 0;
	g_sendCalls = 0; g_notifyCalls = 0; g_notifySliderCalls = 0;
	msg.m_index = 4; msg.m_value = 0x2c;
	h.SetKnob1Value(&msg);
	unsigned int knob1Addr = 4*9 + 0x149;
	check_eq("SetKnob1Value shadowA written", defaultA[knob1Addr], 0x2c);
	check_eq("SetKnob1Value shadowB written", defaultB[knob1Addr], 0x2c);
	check_eq("SetKnob1Value Send called", g_sendCalls, 1);
	check_eq("SetKnob1Value Notify called", g_notifyCalls, 1);
	check_eq("SetKnob1Value index arg", g_indexArg, 4);
	check_eq("SetKnob1Value slider-notify fires (mode!=1)", g_notifySliderCalls, 1);

	*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) = 1;	/* mode==1 -> slider-notify suppressed */
	g_notifySliderCalls = 0;
	h.SetKnob1Value(&msg);
	check_eq("SetKnob1Value slider-notify suppressed (mode==1)", g_notifySliderCalls, 0);
	*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) = 20;	/* restore */

	/* sub-case H (batch 2): SetSw1Value -- RTParm-indirected Send (bool
	 * arg = value!=0) + dual-shadow, no slider-notify tail call */
	g_sysexGetValueReturn = 0;
	unsigned int sw1Addr = 4*9 + 0x148;
	g_sendCalls = 0; g_notifyCalls = 0;
	msg.m_value = 1;
	h.SetSw1Value(&msg);
	check_eq("SetSw1Value shadowA bit0 set", defaultA[sw1Addr] & 1, 1);
	check_eq("SetSw1Value shadowB bit0 set", defaultB[sw1Addr] & 1, 1);
	check_eq("SetSw1Value bool arg true", g_boolArg, 1);
	check_eq("SetSw1Value Send called", g_sendCalls, 1);

	/* sub-case I (batch 2): SetScene -- primary+dual-shadow write, its
	 * own LinkedSceneId-style nibble sync (curSceneIdx sourced from
	 * CKGBankManager, NOT msg->m_index), and the SetScene-only +0x14
	 * Send-suppression guard */
	g_sysexGetValueReturn = 0;
	*(unsigned char *)(*(unsigned char **)CKGBankManager::ms_poInstance + 0x135) = 5;	/* curSceneIdx=5, odd -> high nibble */
	g_sendCalls = 0; g_notifyCalls = 0;
	msg.m_value = 0x03;
	h.m_pendingSceneSendGuard = 0;
	h.SetScene(&msg);
	check_eq("SetScene primary 0x127", g_live[0x127], 0x03);
	check_eq("SetScene shadowA 0x127", defaultA[0x127], 0x03);
	check_eq("SetScene shadowB 0x127", defaultB[0x127], 0x03);
	unsigned int sceneNibbleAddr = 0x2e4 + (5 >> 1);
	check_eq("SetScene nibble high (odd curSceneIdx)", (g_live[sceneNibbleAddr] >> 4) & 0x7, 3);
	check_eq("SetScene Send called (guard NULL)", g_sendCalls, 1);

	g_sendCalls = 0;
	h.m_pendingSceneSendGuard = (void *)1;
	h.SetScene(&msg);
	check_eq("SetScene Send suppressed by +0x14 guard", g_sendCalls, 0);
	h.m_pendingSceneSendGuard = 0;

	/* sub-case J (batch 2): SetLinkedSceneId -- unconditional nibble
	 * write (own idx-derived byteIdx/nibble, independent of SetScene's
	 * curSceneIdx), suppression-gated 0x127 mirror + deviceIndex-direct
	 * Send (no GetRTParmBufferSelectId indirection here) */
	g_sysexGetValueReturn = 0;
	msg.m_index = 5;	/* odd -> high nibble, byteIdx = 5/2 = 2 */
	msg.m_value = 0x06;
	g_sendCalls = 0; g_notifyCalls = 0;
	h.SetLinkedSceneId(&msg);
	unsigned int lsNibbleAddr = 0x2e4 + (5 / 2);
	check_eq("SetLinkedSceneId nibble high", (g_live[lsNibbleAddr] >> 4) & 0x7, 6);
	check_eq("SetLinkedSceneId shadowA nibble", (defaultA[lsNibbleAddr] >> 4) & 0x7, 6);
	check_eq("SetLinkedSceneId shadowB nibble", (defaultB[lsNibbleAddr] >> 4) & 0x7, 6);
	check_eq("SetLinkedSceneId 0x127 mirror", g_live[0x127], 0x06);
	check_eq("SetLinkedSceneId Send deviceIndex-direct arg", g_selectIdArg, msg.m_deviceIndex);
	check_eq("SetLinkedSceneId Send called", g_sendCalls, 1);
	check_eq("SetLinkedSceneId Notify called", g_notifyCalls, 1);

	/* suppressed: nibble write must still fire, 0x127 mirror + Send must not */
	CKGEngine::ms_poInstance[0xb0] = 1;
	g_live[0x127] = 0x00;
	g_sendCalls = 0;
	msg.m_value = 0x07;
	h.SetLinkedSceneId(&msg);
	check_eq("SetLinkedSceneId nibble still unconditional when suppressed", (g_live[lsNibbleAddr] >> 4) & 0x7, 7);
	check_eq("SetLinkedSceneId 0x127 mirror suppressed", g_live[0x127], 0x00);
	check_eq("SetLinkedSceneId Send suppressed", g_sendCalls, 0);
	CKGEngine::ms_poInstance[0xb0] = 0;

	printf("--- Part 3: GetKarmaModule / GetKarmaPerfModuleForSeqBackup ---\n");
	/* mocks return predictable fake addresses: Seq=0x1000+idx,
	 * Combi=0x2000+idx, Program=0x3000+idx -- never dereferenced, only
	 * their integer value is checked */
	CKGModuleParamMsg kmsg;
	memset(&kmsg, 0, sizeof(kmsg));
	kmsg.m_deviceIndex = 2;
	kmsg.m_karmaIndexOrSentinel = 5;

	kmsg.m_kind = 1;	/* Program -- verbatim CKGBankManager return, no +index*0x2e8 */
	void *progResult = h.GetKarmaModule(&kmsg);
	check_eq("GetKarmaModule kind=1 Program (no stride add)", (long)progResult, 0x3000 + 5);

	kmsg.m_kind = 2;	/* Seq -- WITH +deviceIndex*0x2e8 */
	void *seqResult = h.GetKarmaModule(&kmsg);
	check_eq("GetKarmaModule kind=2 Seq (with stride add)", (long)seqResult, 0x1000 + 5 + 2*0x2e8);

	kmsg.m_kind = 0;	/* fallthrough default -- Combi, WITH +deviceIndex*0x2e8 */
	void *combiResult = h.GetKarmaModule(&kmsg);
	check_eq("GetKarmaModule kind=0 fallthrough Combi (with stride add)", (long)combiResult, 0x2000 + 5 + 2*0x2e8);

	CSPREngine::ms_poInstance[0xa] = 0;
	check_eq("GetKarmaPerfModuleForSeqBackup null, gate closed", (long)h.GetKarmaPerfModuleForSeqBackup(&kmsg), 0);
	CSPREngine::ms_poInstance[0xa] = 1;
	kmsg.m_kind = 1;
	check_eq("GetKarmaPerfModuleForSeqBackup null, kind!=2", (long)h.GetKarmaPerfModuleForSeqBackup(&kmsg), 0);
	kmsg.m_kind = 2;
	kmsg.m_karmaIndexOrSentinel = 9;
	void *seqBackup = h.GetKarmaPerfModuleForSeqBackup(&kmsg);
	check_eq("GetKarmaPerfModuleForSeqBackup index path", (long)seqBackup, 0x1000 + 9 + 2*0x2e8);

	/* 0xffff sentinel -> index sourced from CKGBankManager[+0x97c7d4] instead */
	*(unsigned int *)(CKGBankManager::ms_poInstance + 0x97c7d4) = 42;
	kmsg.m_karmaIndexOrSentinel = 0xffff;
	void *seqBackupSentinel = h.GetKarmaPerfModuleForSeqBackup(&kmsg);
	check_eq("GetKarmaPerfModuleForSeqBackup sentinel path", (long)seqBackupSentinel, 0x1000 + 42 + 2*0x2e8);

	printf("========================================================================\n");
	if (g_fail) {
		printf("%d FAIL\n", g_fail);
		return 1;
	}
	printf("all checks passed\n");
	return 0;
}
