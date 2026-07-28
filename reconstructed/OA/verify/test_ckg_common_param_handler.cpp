// SPDX-License-Identifier: GPL-2.0
/*
 * test_ckg_common_param_handler.cpp  -  KAT for CKGCommonParamMsgHandler
 * (see ../src/engine/ckg_common_param_handler.cpp).
 *
 * Part 1: primary (unconditional) field-write check for all 66 mechanical
 * methods -- CSPREngine gate held closed and CKGEngine edit-suppression held
 * closed so ONLY the primary m_liveRecord write executes. Expected values
 * computed independently here from the same offset/stride/shift/mask facts
 * the generator used, not by re-using the .cpp file's own C expressions.
 * msg.m_index fixed at 5 for every ctx-indexed method.
 *
 * Part 2: full gate+shadow-write+Send/Notify skeleton exercise for a
 * representative subset (single-shadow, dual-shadow, 3-arg group-const,
 * 5-arg Knob/Sw, no-send SwName/KnobName).
 *
 * Part 3: the 6 standalone outliers -- GetKarmaPerfCommon (3-way switch +
 * real NULL fallback), GetKarmaPerfCommonForSeqBackup, ShouldStoreToBackup,
 * SetChordMemVelocity (confirmed no-op), SetTempo (suppressed/CheckAndSet
 * true-sync/true-nosync/false branches, incl. the real msg->m_value mutation),
 * SetScene (dual-shadow fixed-offset field + 4-module broadcast loop).
 */

#include <cstdio>
#include <cstring>
#include "oa_ckg_common_param_msg_handler.h"

/* host-only mock storage/bodies for every out-of-scope singleton/dependency */
unsigned char *CSPREngine::ms_poInstance;
unsigned char *CKGBankManager::ms_poInstance;
unsigned char *CKGEngine::ms_poInstance;
CKGParamEdit *CKGEngine::ms_poKGParamEdit;
unsigned char *CKGUIMsgProcessor::ms_poInstance;
CSPRSysExBufManager *CSPRMIDIMsgProcessor::ms_poSysExPlayBuf;
static int g_sendCalls;
static int g_notifyCalls;
static int g_sysexGetValueCalls;
static int g_sysexGetValueReturn;
static int g_lastSendArgs[6];
static int g_moduleSceneCalls;
static int g_lastModuleSceneArgs[2];
static bool g_checkAndSetReturn;
static bool g_shouldSyncReturn;
void CKGParamEdit::SendAssignableSwitch(int a0, int a1, int a2, bool a3, bool a4) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; g_lastSendArgs[2] = (int)a2; g_lastSendArgs[3] = (int)a3; g_lastSendArgs[4] = (int)a4; }
void CKGParamEdit::SendBufferSelect(unsigned char a0) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; }
void CKGParamEdit::SendChordMemChannel(int a0, unsigned char a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendChordMemNote(int a0, int a1, int a2) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; g_lastSendArgs[2] = (int)a2; }
void CKGParamEdit::SendChordMemVelocity(int a0, int a1, unsigned char a2) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; g_lastSendArgs[2] = (int)a2; }
void CKGParamEdit::SendDTRun(unsigned char a0, unsigned char a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendDynAction(unsigned char a0, unsigned char a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendDynBottom(unsigned char a0, unsigned char a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendDynDestination(unsigned char a0, unsigned char a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendDynInputModule(unsigned char a0, unsigned char a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendDynModule(unsigned char a0, unsigned char a1, unsigned char a2) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; g_lastSendArgs[2] = (int)a2; }
void CKGParamEdit::SendDynPolarity(unsigned char a0, unsigned char a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendDynSource(unsigned char a0, unsigned char a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendDynTop(unsigned char a0, unsigned char a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendKnob(int a0, int a1, int a2, int a3, bool a4) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; g_lastSendArgs[2] = (int)a2; g_lastSendArgs[3] = (int)a3; g_lastSendArgs[4] = (int)a4; }
void CKGParamEdit::SendLatch(bool a0) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; }
void CKGParamEdit::SendNoteMapTable(int a0, int a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendOnOff(bool a0) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; }
void CKGParamEdit::SendPadMode(unsigned char a0) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; }
void CKGParamEdit::SendRTPDestKnob(unsigned char a0, char a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendRTPMaxValue(unsigned char a0, short a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendRTPMinValue(unsigned char a0, short a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendRTPModule(unsigned char a0, unsigned char a1, unsigned char a2) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; g_lastSendArgs[2] = (int)a2; }
void CKGParamEdit::SendRTPParamAssign(unsigned char a0, unsigned char a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendRTPParamGroup(unsigned char a0, unsigned char a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendRTPPolarity(unsigned char a0, char a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendRTPValue(unsigned char a0, short a1) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; }
void CKGParamEdit::SendScene(int a0, unsigned char a1, bool a2) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; g_lastSendArgs[1] = (int)a1; g_lastSendArgs[2] = (int)a2; }
void CKGParamEdit::SendSceneChangeQuantize(int a0) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; }
void CKGParamEdit::SendTempo(unsigned short a0) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; }
void CKGParamEdit::SendTimeSig(unsigned char a0) { g_sendCalls++; g_lastSendArgs[0] = (int)a0; }
void CKGUIMsgProcessor::NotifyAfterEdit() { g_notifyCalls++; }
void CKGUIMsgProcessor::NotifyAfterEdit(bool, int) { g_notifyCalls++; }
void CKGUIMsgProcessor::SendModuleSceneMessage(int a, int b) { g_moduleSceneCalls++; g_lastModuleSceneArgs[0]=a; g_lastModuleSceneArgs[1]=b; }
char CSPRSysExBufManager::GetValue(int,int,int,int,int,int,int,long*) { g_sysexGetValueCalls++; return (char)g_sysexGetValueReturn; }
unsigned char *CKGBankManager::GetSeqKarmaPerfCommon(unsigned int idx) { static unsigned char buf[0x1000]; return buf + idx; }
unsigned char *CKGBankManager::GetSeqKarmaPerfModule(unsigned int) { return 0; }
unsigned char *CKGBankManager::GetSeqDefaultKarmaPerfCommon() { return 0; }
unsigned char *CKGBankManager::GetCombiKarmaPerfModule(eSTGCombiBankId, unsigned int) { return 0; }
unsigned char *CKGBankManager::GetProgKarmaPerfModule(eSTGProgramBankId, unsigned int) { return 0; }
unsigned char *CKGBankManager::GetProgKarmaPerfCommon(eSTGProgramBankId, unsigned int idx) { static unsigned char buf[0x1000]; return buf + idx*3; }
unsigned char *CKGBankManager::GetCombiKarmaPerfCommon(eSTGCombiBankId, unsigned int idx) { static unsigned char buf[0x1000]; return buf + idx*5; }
bool KGOutGate_CheckAndSetTempoForOtherModule(int) { return g_checkAndSetReturn; }
CTimerManager *CTimerManager::ms_poInstance;
bool CTimerManager::ShouldSyncExternalClock() { return g_shouldSyncReturn; }

static int g_fail;
static void check_eq(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-45s %ld\n", label, got); return; }
	printf("  FAIL  %-45s got=%ld want=%ld\n", label, got, want);
	g_fail++;
}

#define BUFSZ 0x800
static unsigned char g_live[BUFSZ];
static unsigned char g_defA[BUFSZ];
static unsigned char g_defB[BUFSZ];

int main(void)
{
	printf("CKGCommonParamMsgHandler known-answer test\n");
	printf("========================================================================\n");

	CSPREngine::ms_poInstance = new unsigned char[0x10](); /* [0xa]=0: gate closed */
	CKGEngine::ms_poInstance = new unsigned char[0x100](); CKGEngine::ms_poInstance[0xb0] = 1; /* suppressed */
	CKGUIMsgProcessor::ms_poInstance = new unsigned char[0x100]();
	CKGBankManager::ms_poInstance = new unsigned char[0x1000000]();

	CKGCommonParamMsgHandler h;
	memset(&h, 0, sizeof(h));
	h.m_liveRecord = g_live;
	h.m_defaultRecordA = 0;
	h.m_defaultRecordB = 0;

	CKGCommonParamMsg msg;
	memset(&msg, 0, sizeof(msg));
	msg.m_index = 5;

	printf("--- Part 1: primary field write (gates closed) ---\n");
	for (unsigned int i = 0; i < BUFSZ; i++) g_live[i] = (unsigned char)(i*0x53 + 0x81);

	msg.m_value = 305441741;
	h.SetTimeSig(&msg);
	check_eq("SetTimeSig", (unsigned char)*(unsigned char*)(g_live+0x3), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x2];
	  h.SetPadMode(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 5)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 5));
	  check_eq("SetPadMode", g_live[0x2], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x2];
	  h.SetModuleControl(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x7 << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x7) << 0));
	  check_eq("SetModuleControl", g_live[0x2], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x2];
	  h.SetLatch(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 6)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 6));
	  check_eq("SetLatch", g_live[0x2], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x2];
	  h.SetOnOff(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 7)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 7));
	  check_eq("SetOnOff", g_live[0x2], want); }
	msg.m_value = 305441741;
	h.SetSwName(&msg);
	check_eq("SetSwName", (unsigned short)*(unsigned short*)(g_live+0xe), (unsigned short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetKnobName(&msg);
	check_eq("SetKnobName", (unsigned short)*(unsigned short*)(g_live+0x1e), (unsigned short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetNoteMapTableValue(&msg);
	check_eq("SetNoteMapTableValue", (signed char)*(signed char*)(g_live+0x183), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x135];
	  h.SetSceneChangeQuantizeValue(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0xf << 4)) | ((unsigned char)(((unsigned int)305441741) & 0xf) << 4));
	  check_eq("SetSceneChangeQuantizeValue", g_live[0x135], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x42];
	  h.SetDynMIDIInput(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x7 << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x7) << 0));
	  check_eq("SetDynMIDIInput", g_live[0x42], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x42];
	  h.SetDynMIDIPolarity(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x3 << 3)) | ((unsigned char)(((unsigned int)305441741) & 0x3) << 3));
	  check_eq("SetDynMIDIPolarity", g_live[0x42], want); }
	msg.m_value = 305441741;
	h.SetDynMIDISource(&msg);
	check_eq("SetDynMIDISource", (unsigned char)*(unsigned char*)(g_live+0x43), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetDynMIDIDest(&msg);
	check_eq("SetDynMIDIDest", (unsigned char)*(unsigned char*)(g_live+0x44), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x45];
	  h.SetDynMIDIUseA(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 0));
	  check_eq("SetDynMIDIUseA", g_live[0x45], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x45];
	  h.SetDynMIDIUseB(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 1)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 1));
	  check_eq("SetDynMIDIUseB", g_live[0x45], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x45];
	  h.SetDynMIDIUseC(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 2)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 2));
	  check_eq("SetDynMIDIUseC", g_live[0x45], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x45];
	  h.SetDynMIDIUseD(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 3)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 3));
	  check_eq("SetDynMIDIUseD", g_live[0x45], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x45];
	  h.SetDynMIDIUseLast(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 4)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 4));
	  check_eq("SetDynMIDIUseLast", g_live[0x45], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x45];
	  h.SetDynMIDIUseAction(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x3 << 5)) | ((unsigned char)(((unsigned int)305441741) & 0x3) << 5));
	  check_eq("SetDynMIDIUseAction", g_live[0x45], want); }
	msg.m_value = 305441741;
	h.SetDynMIDIUseTop(&msg);
	check_eq("SetDynMIDIUseTop", (unsigned char)*(unsigned char*)(g_live+0x46), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetDynMIDIUseBottom(&msg);
	check_eq("SetDynMIDIUseBottom", (unsigned char)*(unsigned char*)(g_live+0x47), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetRTParamGroup(&msg);
	check_eq("SetRTParamGroup", (unsigned char)*(unsigned char*)(g_live+0x86), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetRTParamAssign(&msg);
	check_eq("SetRTParamAssign", (unsigned char)*(unsigned char*)(g_live+0x87), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x88];
	  h.SetRTParamA(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 0));
	  check_eq("SetRTParamA", g_live[0x88], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x88];
	  h.SetRTParamB(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 1)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 1));
	  check_eq("SetRTParamB", g_live[0x88], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x88];
	  h.SetRTParamC(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 2)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 2));
	  check_eq("SetRTParamC", g_live[0x88], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x88];
	  h.SetRTParamD(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 3)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 3));
	  check_eq("SetRTParamD", g_live[0x88], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x88];
	  h.SetRTParamPolarity(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x7 << 4)) | ((unsigned char)(((unsigned int)305441741) & 0x7) << 4));
	  check_eq("SetRTParamPolarity", g_live[0x88], want); }
	msg.m_value = 305441741;
	h.SetRTParamKnob(&msg);
	check_eq("SetRTParamKnob", (signed char)*(signed char*)(g_live+0x89), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetRTParamMin(&msg);
	check_eq("SetRTParamMin", (short)*(short*)(g_live+0x8a), (short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetRTParamMax(&msg);
	check_eq("SetRTParamMax", (short)*(short*)(g_live+0x8c), (short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetRTParamValue(&msg);
	check_eq("SetRTParamValue", (short)*(short*)(g_live+0x8e), (short)(unsigned short)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote1(&msg);
	check_eq("SetChordMemNote1", (signed char)*(signed char*)(g_live+0xfe), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote2(&msg);
	check_eq("SetChordMemNote2", (signed char)*(signed char*)(g_live+0x100), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote3(&msg);
	check_eq("SetChordMemNote3", (signed char)*(signed char*)(g_live+0x102), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote4(&msg);
	check_eq("SetChordMemNote4", (signed char)*(signed char*)(g_live+0x104), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote5(&msg);
	check_eq("SetChordMemNote5", (signed char)*(signed char*)(g_live+0x106), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote6(&msg);
	check_eq("SetChordMemNote6", (signed char)*(signed char*)(g_live+0x108), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote7(&msg);
	check_eq("SetChordMemNote7", (signed char)*(signed char*)(g_live+0x10a), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote8(&msg);
	check_eq("SetChordMemNote8", (signed char)*(signed char*)(g_live+0x10c), (signed char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote1Vel(&msg);
	check_eq("SetChordMemNote1Vel", (unsigned char)*(unsigned char*)(g_live+0xff), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote2Vel(&msg);
	check_eq("SetChordMemNote2Vel", (unsigned char)*(unsigned char*)(g_live+0x101), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote3Vel(&msg);
	check_eq("SetChordMemNote3Vel", (unsigned char)*(unsigned char*)(g_live+0x103), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote4Vel(&msg);
	check_eq("SetChordMemNote4Vel", (unsigned char)*(unsigned char*)(g_live+0x105), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote5Vel(&msg);
	check_eq("SetChordMemNote5Vel", (unsigned char)*(unsigned char*)(g_live+0x107), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote6Vel(&msg);
	check_eq("SetChordMemNote6Vel", (unsigned char)*(unsigned char*)(g_live+0x109), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote7Vel(&msg);
	check_eq("SetChordMemNote7Vel", (unsigned char)*(unsigned char*)(g_live+0x10b), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemNote8Vel(&msg);
	check_eq("SetChordMemNote8Vel", (unsigned char)*(unsigned char*)(g_live+0x10d), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetChordMemChannel(&msg);
	check_eq("SetChordMemChannel", (unsigned char)*(unsigned char*)(g_live+0x10f), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x163];
	  h.SetSw1Value(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 0)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 0));
	  check_eq("SetSw1Value", g_live[0x163], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x163];
	  h.SetSw2Value(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 1)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 1));
	  check_eq("SetSw2Value", g_live[0x163], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x163];
	  h.SetSw3Value(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 2)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 2));
	  check_eq("SetSw3Value", g_live[0x163], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x163];
	  h.SetSw4Value(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 3)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 3));
	  check_eq("SetSw4Value", g_live[0x163], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x163];
	  h.SetSw5Value(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 4)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 4));
	  check_eq("SetSw5Value", g_live[0x163], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x163];
	  h.SetSw6Value(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 5)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 5));
	  check_eq("SetSw6Value", g_live[0x163], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x163];
	  h.SetSw7Value(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 6)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 6));
	  check_eq("SetSw7Value", g_live[0x163], want); }
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x163];
	  h.SetSw8Value(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 7)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 7));
	  check_eq("SetSw8Value", g_live[0x163], want); }
	msg.m_value = 305441741;
	h.SetKnob1Value(&msg);
	check_eq("SetKnob1Value", (unsigned char)*(unsigned char*)(g_live+0x164), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetKnob2Value(&msg);
	check_eq("SetKnob2Value", (unsigned char)*(unsigned char*)(g_live+0x165), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetKnob3Value(&msg);
	check_eq("SetKnob3Value", (unsigned char)*(unsigned char*)(g_live+0x166), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetKnob4Value(&msg);
	check_eq("SetKnob4Value", (unsigned char)*(unsigned char*)(g_live+0x167), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetKnob5Value(&msg);
	check_eq("SetKnob5Value", (unsigned char)*(unsigned char*)(g_live+0x168), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetKnob6Value(&msg);
	check_eq("SetKnob6Value", (unsigned char)*(unsigned char*)(g_live+0x169), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetKnob7Value(&msg);
	check_eq("SetKnob7Value", (unsigned char)*(unsigned char*)(g_live+0x16a), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	h.SetKnob8Value(&msg);
	check_eq("SetKnob8Value", (unsigned char)*(unsigned char*)(g_live+0x16b), (unsigned char)(unsigned char)((unsigned int)305441741));
	msg.m_value = 305441741;
	{ unsigned char pre = g_live[0x134];
	  h.SetDTRun(&msg);
	  unsigned char want = (unsigned char)((pre & (unsigned char)~(0x1 << 5)) | ((unsigned char)(((unsigned int)305441741) & 0x1) << 5));
	  check_eq("SetDTRun", g_live[0x134], want); }
	printf("--- Part 2: full gate+shadow+Send/Notify skeleton (representative subset) ---\n");

	CSPREngine::ms_poInstance[0xa] = 1;                 /* SysEx-shadow gate open */
	h.m_defaultRecordA = g_defA;
	h.m_defaultRecordB = g_defB;
	CKGUIMsgProcessor::ms_poInstance[0x6c] = 0;
	*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) = 20; /* mode outside {8,9,10}: shadow attempted */
	CKGEngine::ms_poInstance[0xb0] = 0;                 /* edits NOT suppressed: Send+Notify fire */
	CKGEngine::ms_poKGParamEdit = (CKGParamEdit *)1;    /* non-null "this", never dereferenced by our stubs */
	g_sysexGetValueReturn = 0;                          /* GetValue() reports a miss: shadow write happens */

	for (unsigned int i = 0; i < BUFSZ; i++) { g_live[i] = 0; g_defA[i] = 0; g_defB[i] = 0; }

	/* SetTimeSig: single-shadow, 1-arg Send(value), non-ctx, fixed offset 0x3 */
	{
		g_sendCalls = g_notifyCalls = g_sysexGetValueCalls = 0;
		msg.m_value = 0x42;
		h.SetTimeSig(&msg);
		check_eq("SetTimeSig live", g_live[0x3], 0x42);
		check_eq("SetTimeSig shadowA", g_defA[0x3], 0x42);
		check_eq("SetTimeSig shadowB untouched (single-shadow)", g_defB[0x3], 0);
		check_eq("SetTimeSig sysex GetValue called", g_sysexGetValueCalls, 1);
		check_eq("SetTimeSig Send called", g_sendCalls, 1);
		check_eq("SetTimeSig Send arg0 == value", g_lastSendArgs[0], 0x42);
		check_eq("SetTimeSig Notify called", g_notifyCalls, 1);
	}

	/* SetChordMemChannel: single-shadow, 2-arg Send(index,value), ctx-indexed stride18 off 0xb5 */
	{
		g_sendCalls = g_notifyCalls = 0;
		msg.m_value = 7;
		h.SetChordMemChannel(&msg);
		unsigned addr = 0xb5 + 5*18;
		check_eq("SetChordMemChannel live", g_live[addr], 7);
		check_eq("SetChordMemChannel shadowA", g_defA[addr], 7);
		check_eq("SetChordMemChannel shadowB untouched", g_defB[addr], 0);
		check_eq("SetChordMemChannel Send arg0 == index", g_lastSendArgs[0], 5);
		check_eq("SetChordMemChannel Send arg1 == value", g_lastSendArgs[1], 7);
	}

	/* SetDynMIDIUseB: single-shadow, 3-arg Send(index,const=1,value), bitfield off 0x27 shift1 mask1 */
	{
		g_sendCalls = 0;
		unsigned addr = 0x27 + 5*6;  /* stride 6, INDEX=5 */
		g_live[addr] = 0xff; g_defA[addr] = 0xff;
		msg.m_value = 0;
		h.SetDynMIDIUseB(&msg);
		check_eq("SetDynMIDIUseB live bit cleared", (g_live[addr] >> 1) & 1, 0);
		check_eq("SetDynMIDIUseB live neighbors preserved", g_live[addr] | 2, 0xff);
		check_eq("SetDynMIDIUseB shadowA bit cleared", (g_defA[addr] >> 1) & 1, 0);
		check_eq("SetDynMIDIUseB Send arg1 == group const 1", g_lastSendArgs[1], 1);
	}

	/* SetRTParamA: DUAL-shadow (m_default-based), 3-arg Send(index,const=0,value), bitfield off 0x56 shift0 mask1 */
	{
		unsigned addr = 0x56 + 5*10;
		g_live[addr] = 0; g_defA[addr] = 0; g_defB[addr] = 0;
		msg.m_value = 1;
		h.SetRTParamA(&msg);
		check_eq("SetRTParamA live bit set", g_live[addr] & 1, 1);
		check_eq("SetRTParamA shadowA bit set", g_defA[addr] & 1, 1);
		check_eq("SetRTParamA shadowB bit set (dual-shadow)", g_defB[addr] & 1, 1);
		check_eq("SetRTParamA Send arg1 == group const 0", g_lastSendArgs[1], 0);
	}

	/* SetKnob1Value: DUAL-shadow, 5-arg SendKnob(0,index,0,value,false) */
	{
		g_sendCalls = 0;
		unsigned addr = 0x137 + 5*9;
		msg.m_value = 99;
		h.SetKnob1Value(&msg);
		check_eq("SetKnob1Value live", g_live[addr], 99);
		check_eq("SetKnob1Value shadowA", g_defA[addr], 99);
		check_eq("SetKnob1Value shadowB (dual-shadow)", g_defB[addr], 99);
		check_eq("SetKnob1Value SendKnob called", g_sendCalls, 1);
		check_eq("SetKnob1Value SendKnob arg0 == const 0", g_lastSendArgs[0], 0);
		check_eq("SetKnob1Value SendKnob arg1 == index", g_lastSendArgs[1], 5);
	}

	/* SetSw1Value: DUAL-shadow, 5-arg SendAssignableSwitch(0,index,0,bool,false), bitfield off 0x136 shift0 mask1 */
	{
		g_sendCalls = 0;
		unsigned addr = 0x136 + 5*9;
		g_live[addr] = 0; g_defA[addr] = 0; g_defB[addr] = 0;
		msg.m_value = 1;
		h.SetSw1Value(&msg);
		check_eq("SetSw1Value live bit set", g_live[addr] & 1, 1);
		check_eq("SetSw1Value shadowB bit set (dual-shadow)", g_defB[addr] & 1, 1);
		check_eq("SetSw1Value SendAssignableSwitch called", g_sendCalls, 1);
	}

	/* SetSwName: NO Send call, DUAL-shadow, but NotifyAfterEdit still fires when unsuppressed */
	{
		g_sendCalls = g_notifyCalls = 0;
		unsigned addr = 0x4 + 5*2;
		msg.m_value = 0xabcd;
		h.SetSwName(&msg);
		check_eq("SetSwName live", *(unsigned short *)(g_live+addr), (unsigned short)0xabcd);
		check_eq("SetSwName shadowB (dual-shadow)", *(unsigned short *)(g_defB+addr), (unsigned short)0xabcd);
		check_eq("SetSwName no Send call", g_sendCalls, 0);
		check_eq("SetSwName Notify still fires", g_notifyCalls, 1);
	}

	/* SetDTRun: single-shadow, dynamic per-index bit (shift == msg.m_index), fixed byte 0x134 */
	{
		unsigned addr = 0x134;
		g_live[addr] = 0; g_defA[addr] = 0;
		msg.m_value = 1;
		h.SetDTRun(&msg);
		check_eq("SetDTRun live bit at m_index", (g_live[addr] >> 5) & 1, 1);
		check_eq("SetDTRun shadowA bit at m_index", (g_defA[addr] >> 5) & 1, 1);
		check_eq("SetDTRun Send arg0 == index", g_lastSendArgs[0], 5);
	}

	printf("--- Part 2b: gate CLOSED via CSPREngine ---\n");
	{
		CSPREngine::ms_poInstance[0xa] = 0;   /* gate shut: no shadow write regardless of mode */
		g_sysexGetValueCalls = 0;
		unsigned addr = 0x3;
		g_defA[addr] = 0x55;
		msg.m_value = 0x77;
		h.SetTimeSig(&msg);
		check_eq("SetTimeSig gate-shut: no GetValue call", g_sysexGetValueCalls, 0);
		check_eq("SetTimeSig gate-shut: shadowA untouched", g_defA[addr], 0x55);
		CSPREngine::ms_poInstance[0xa] = 1;
	}

	printf("--- Part 2c: shadow gate open but SysEx GetValue reports a HIT (no shadow write) ---\n");
	{
		g_sysexGetValueReturn = 1;   /* hit: shadow write skipped */
		unsigned addr = 0x3;
		g_defA[addr] = 0x11;
		msg.m_value = 0x22;
		h.SetTimeSig(&msg);
		check_eq("SetTimeSig hit: live still written", g_live[addr], 0x22);
		check_eq("SetTimeSig hit: shadowA untouched", g_defA[addr], 0x11);
		g_sysexGetValueReturn = 0;
	}

	printf("--- Part 2d: mode INSIDE {8,9,10} -- gate closed (post-fix, correct sense) ---\n");
	{
		*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) = 9;
		g_sysexGetValueCalls = 0;
		unsigned addr = 0x3;
		g_defA[addr] = 0x33;
		msg.m_value = 0x44;
		h.SetTimeSig(&msg);
		check_eq("SetTimeSig mode=9: no GetValue call (gate shut)", g_sysexGetValueCalls, 0);
		check_eq("SetTimeSig mode=9: shadowA untouched", g_defA[addr], 0x33);
		*(int *)(CKGUIMsgProcessor::ms_poInstance + 0x6c) = 20;
	}

	printf("--- Part 3: standalone outliers ---\n");

	/* GetKarmaPerfCommon: 3-way switch + real NULL fallback for unknown m_kind */
	{
		CKGCommonParamMsg km; memset(&km, 0, sizeof(km));
		km.m_kind = 1; km.m_bankId = 2; km.m_karmaIndexOrSentinel = 4;
		void *r1 = h.GetKarmaPerfCommon(&km);
		check_eq("GetKarmaPerfCommon kind=1 (Program) non-null", r1 != 0, true);
		km.m_kind = 2; km.m_karmaIndexOrSentinel = 7;
		void *r2 = h.GetKarmaPerfCommon(&km);
		check_eq("GetKarmaPerfCommon kind=2 (Seq) non-null", r2 != 0, true);
		km.m_kind = 0; km.m_bankId = 3; km.m_karmaIndexOrSentinel = 9;
		void *r3 = h.GetKarmaPerfCommon(&km);
		check_eq("GetKarmaPerfCommon kind=0 (Combi) non-null", r3 != 0, true);
		km.m_kind = 42; /* real explicit NULL fallback */
		void *r4 = h.GetKarmaPerfCommon(&km);
		check_eq("GetKarmaPerfCommon unknown kind returns NULL", (long)r4, 0);
	}

	/* GetKarmaPerfCommonForSeqBackup: CSPREngine gate + kind==2-only + sentinel */
	{
		CKGCommonParamMsg km; memset(&km, 0, sizeof(km));
		km.m_kind = 1; /* wrong kind */
		check_eq("GetKarmaPerfCommonForSeqBackup wrong kind", (long)h.GetKarmaPerfCommonForSeqBackup(&km), 0);
		km.m_kind = 2; km.m_karmaIndexOrSentinel = 3;
		void *r = h.GetKarmaPerfCommonForSeqBackup(&km);
		(void)r;
		CSPREngine::ms_poInstance[0xa] = 0;
		check_eq("GetKarmaPerfCommonForSeqBackup gate shut", (long)h.GetKarmaPerfCommonForSeqBackup(&km), 0);
		CSPREngine::ms_poInstance[0xa] = 1;
	}

	/* ShouldStoreToBackup: reuses the same gate, returns bool instead of writing */
	{
		g_sysexGetValueReturn = 0; /* miss -> true */
		CKGCommonParamMsg km; memset(&km, 0, sizeof(km));
		check_eq("ShouldStoreToBackup miss->true", h.ShouldStoreToBackup(&km), true);
		g_sysexGetValueReturn = 1; /* hit -> false */
		check_eq("ShouldStoreToBackup hit->false", h.ShouldStoreToBackup(&km), false);
		g_sysexGetValueReturn = 0;
	}

	/* SetChordMemVelocity: confirmed real no-op */
	{
		for (unsigned int i = 0; i < BUFSZ; i++) g_live[i] = 0xAA;
		CKGCommonParamMsg km; memset(&km, 0, sizeof(km));
		km.m_value = 55;
		h.SetChordMemVelocity(&km);
		bool unchanged = true;
		for (unsigned int i = 0; i < BUFSZ; i++) if (g_live[i] != 0xAA) unchanged = false;
		check_eq("SetChordMemVelocity real no-op: live untouched", unchanged, true);
	}

	/* SetTempo: 3 real paths */
	{
		for (unsigned int i = 0; i < BUFSZ; i++) { g_live[i] = 0; g_defA[i] = 0; }
		*(unsigned short *)g_live = 120; /* old tempo */

		/* path A: suppressed -- plain mirror write, no gate/Send/Notify at all */
		CKGEngine::ms_poInstance[0xb0] = 1;
		CKGCommonParamMsg tm; memset(&tm, 0, sizeof(tm));
		tm.m_value = 140;
		h.SetTempo(&tm);
		check_eq("SetTempo suppressed: live mirrors new value", *(unsigned short*)g_live, 140);

		/* path B: not suppressed, CheckAndSet true, ShouldSync false -> real write + Notify(true, old) */
		CKGEngine::ms_poInstance[0xb0] = 0;
		*(unsigned short *)g_live = 120;
		g_checkAndSetReturn = true; g_shouldSyncReturn = false;
		g_sendCalls = g_notifyCalls = 0;
		CKGCommonParamMsg tm2; memset(&tm2, 0, sizeof(tm2));
		tm2.m_value = 150;
		h.SetTempo(&tm2);
		check_eq("SetTempo path-B: live updated to new tempo", *(unsigned short*)g_live, 150);
		check_eq("SetTempo path-B: shadowA updated (single, unconditional)", *(unsigned short*)g_defA, 150);
		check_eq("SetTempo path-B: SendTempo called", g_sendCalls, 1);
		check_eq("SetTempo path-B: Notify(true,old) fired", g_notifyCalls, 1);
		check_eq("SetTempo path-B: msg->m_value NOT mutated", tm2.m_value, 150);

		/* path C: not suppressed, CheckAndSet true, ShouldSync true -> falls through to "old value" tail */
		*(unsigned short *)g_live = 120;
		g_checkAndSetReturn = true; g_shouldSyncReturn = true;
		g_sendCalls = g_notifyCalls = 0;
		CKGCommonParamMsg tm3; memset(&tm3, 0, sizeof(tm3));
		tm3.m_value = 160;
		h.SetTempo(&tm3);
		check_eq("SetTempo path-C: live NOT updated (still old)", *(unsigned short*)g_live, 120);
		check_eq("SetTempo path-C: msg->m_value mutated to OLD tempo", tm3.m_value, 120);
		check_eq("SetTempo path-C: Notify(false,old) fired", g_notifyCalls, 1);

		/* path D: not suppressed, CheckAndSet false -> "old value" tail, no record write, no Send */
		*(unsigned short *)g_live = 120;
		g_checkAndSetReturn = false;
		g_sendCalls = g_notifyCalls = 0;
		CKGCommonParamMsg tm4; memset(&tm4, 0, sizeof(tm4));
		tm4.m_value = 170;
		h.SetTempo(&tm4);
		check_eq("SetTempo path-D: live NOT updated", *(unsigned short*)g_live, 120);
		check_eq("SetTempo path-D: msg->m_value mutated to OLD tempo", tm4.m_value, 120);
		check_eq("SetTempo path-D: no Send call", g_sendCalls, 0);
		check_eq("SetTempo path-D: Notify(false,old) fired", g_notifyCalls, 1);
	}

	/* SetScene: fixed-offset dual-shadow field + 4-module broadcast loop */
	{
		for (unsigned int i = 0; i < BUFSZ; i++) { g_live[i] = 0; g_defA[i] = 0; g_defB[i] = 0; }
		CKGEngine::ms_poInstance[0xb0] = 0;
		g_sysexGetValueReturn = 0; /* miss: shadow write happens */

		/* build a fake CKGBankManager 0x4 pointer -> module-array base */
		static unsigned char modArray[4 * 0x2e8];
		memset(modArray, 0, sizeof(modArray));
		*(unsigned char **)(CKGBankManager::ms_poInstance + 0x4) = modArray;

		/* module 0: participates (bit 0x8 set), even index -> low nibble */
		modArray[0x2e4] = 0x8;
		modArray[0x2e4 + 3] = 0x05; /* nibIdx = 0x06>>1 = 3 for msg.m_value=6 */
		/* module 1: participates, odd index -> high nibble */
		unsigned char *mod1 = modArray + 0x2e8;
		mod1[0x2e4] = 0x8;
		/* module 2: does NOT participate */
		unsigned char *mod2 = modArray + 2*0x2e8;
		mod2[0x2e4] = 0x0;
		/* module 3: does NOT participate */
		unsigned char *mod3 = modArray + 3*0x2e8;
		mod3[0x2e4] = 0x0;

		g_sendCalls = g_notifyCalls = g_moduleSceneCalls = 0;
		CKGCommonParamMsg sm; memset(&sm, 0, sizeof(sm));
		sm.m_value = 0x06; /* low 3 bits = 6 (primary write), even -> nibIdx=3, bit0=0 (low nibble) */
		mod1[0x2e4 + 3] = 0x27; /* module1's own nibIdx=3 byte; low nibble irrelevant since value even->module0 uses its own nibIdx; but sm.m_value even applies to ALL modules identically (same msg) */
		h.SetScene(&sm);

		check_eq("SetScene live low3 bits", g_live[0x135] & 0x7, 6);
		check_eq("SetScene shadowA (dual)", g_defA[0x135] & 0x7, 6);
		check_eq("SetScene shadowB (dual)", g_defB[0x135] & 0x7, 6);
		check_eq("SetScene SendScene called", g_sendCalls, 1);
		check_eq("SetScene Notify called", g_notifyCalls, 1);
		check_eq("SetScene broadcasts exactly 2 participating modules", g_moduleSceneCalls, 2);

		/* suppressed: Send/Notify AND broadcast loop all skipped entirely */
		CKGEngine::ms_poInstance[0xb0] = 1;
		g_sendCalls = g_notifyCalls = g_moduleSceneCalls = 0;
		CKGCommonParamMsg sm2; memset(&sm2, 0, sizeof(sm2));
		sm2.m_value = 3;
		h.SetScene(&sm2);
		check_eq("SetScene suppressed: live still written", g_live[0x135] & 0x7, 3);
		check_eq("SetScene suppressed: no Send", g_sendCalls, 0);
		check_eq("SetScene suppressed: no broadcast", g_moduleSceneCalls, 0);
		CKGEngine::ms_poInstance[0xb0] = 0;
	}

	printf("========================================================================\n");
	if (g_fail) {
		printf("%d CHECK(S) FAILED\n", g_fail);
		return 1;
	}
	printf("ALL CHECKS PASSED\n");
	return 0;
}
