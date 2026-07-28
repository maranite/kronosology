// SPDX-License-Identifier: GPL-2.0
/*
 * test_ckg_midi_msg_handler.cpp  -  KAT for CSKMIDIMsgHandler /
 * CSKSpecialMsgHandler / CSKSysExMsgHandler (see
 * ../src/engine/ckg_midi_msg_handler.cpp).
 *
 * Two kinds of check:
 *
 *  1. STRUCTURAL: CSKSysExMsgHandler is driven through a
 *     `CSKMIDIMsgHandler*` base pointer to confirm real virtual dispatch
 *     (single-inheritance-only ABI, no diamond/VTT concerns here, but
 *     still worth proving the vtable layout matches).
 *  2. INDEPENDENT ORACLE: every non-trivial arithmetic KAT value below
 *     (ConvertPostMIDINote's octave-wrap transpose, CheckAndGetCorrectCCValue's
 *     CSWTCH_42 table lookup, ProcessPitchBendMessage's 2-value transform,
 *     Analize()'s SOX/EOX state machine, CopyToBuffer()'s 0x20 wraparound)
 *     was computed by a standalone Python re-implementation of the same
 *     algorithm straight from the raw x86 disassembly notes (scratchpad
 *     oracle_ckg_midi.py, NOT copy-pasted from the .cpp under test) before
 *     being hand-transcribed here as literal expected values -- see that
 *     script's own output for the derivation.
 */

#include <cstdio>
#include <cstring>
#include "oa_ckg_midi_msg_handler.h"

static int g_fail;
static void check(const char *label, long got, long want)
{
	if (got == want) { printf("  ok    %-56s %ld (0x%lx)\n", label, got, (unsigned long)got); return; }
	printf("  FAIL  %-56s got=%ld(0x%lx) want=%ld(0x%lx)\n", label, got, (unsigned long)got, want, (unsigned long)want);
	g_fail++;
}

/* CSKSpecialMsgHandler::m_NowHandlingSamplingPerformanceChange is normally
 * defined in ckg_ui_msg_sender.cpp (not linked into this standalone test),
 * so it needs its own definition here -- same convention as every other
 * mocked singleton/static below. */
bool CSKSpecialMsgHandler::m_NowHandlingSamplingPerformanceChange;

/* ==================== CKGChordTrigger alias targets ==================== */
bool g_CKGChordTrigger_ms_bNowGenaratingChordNotes asm("_ZN15CKGChordTrigger27ms_bNowGenaratingChordNotesE");
bool g_CKGChordTrigger_ms_bNowSendingCCOrNote asm("_ZN15CKGChordTrigger22ms_bNowSendingCCOrNoteE");

/* ==================== mock singleton storage ==================== */

#define BANKSZ 0x97c800
static unsigned char g_bankBuf[BANKSZ];
unsigned char *CKGBankManager::ms_poInstance = g_bankBuf;

unsigned char *CKGEngine::ms_poInstance;
static unsigned char g_engineBuf[0x40];
unsigned char *CDrumTrackBankManager::ms_poInstance;
unsigned char *CSPREngine::ms_poInstance;
unsigned char *CMIDIFlowParamHolder::ms_poThis;
unsigned char *CKGRTCHandler::ms_poInstance;
unsigned char *CSKMIDIMsgProcessor::ms_poInstance;
unsigned char *CKGMIDIMsgProcessor::ms_poInstance;
unsigned char *CSPRClockHandler::ms_poInstance;
unsigned char CSPRClockHandler::ms_oStatusMaster;
CSPRSysExBufManager *CSPRMIDIMsgProcessor::ms_poSysExPlayBuf;
CSTGMessageProcessor *CSTGMessageProcessor::sInstance;
static unsigned char g_stgMsgProcBuf[0x1041];

/* ==================== mocked out-of-scope call counters ==================== */

static int g_changePerformanceCalls;
static int g_lastChangePerfType;
void CKGEngine::ChangePerformance(eSTGMsgPerfType type, bool) { g_changePerformanceCalls++; g_lastChangePerfType = type; }
static bool g_shouldKeepKarma = true;
bool CKGEngine::ShouldKeepKarmaPerformance() { return g_shouldKeepKarma; }
static int g_clearSchedulerCalls, g_karmaOnCalls, g_karmaOffCalls, g_resetLocalCtrlCalls, g_sendShutUpCalls, g_sendCCOffsetBackCalls;
void CKGEngine::ClearScheduler() { g_clearSchedulerCalls++; }
void CKGEngine::KarmaTurnOnWhenFinishDump() { g_karmaOnCalls++; }
void CKGEngine::KarmaTurnOffWhenStartDump() { g_karmaOffCalls++; }
void CKGEngine::ResetLocalController() { g_resetLocalCtrlCalls++; }
void CKGEngine::SendShutUp() { g_sendShutUpCalls++; }
void CKGEngine::SendCCOffsetBack() { g_sendCCOffsetBackCalls++; }
static int g_lastBendRangeChannel = -1;
static unsigned int g_lastBendRangeA, g_lastBendRangeB;
void CKGEngine::SetBendRange(int range, unsigned int a, unsigned int b)
{
	g_lastBendRangeChannel = range;
	g_lastBendRangeA = a;
	g_lastBendRangeB = b;
}

static unsigned int g_lastCombiBankId = 0xdead, g_lastCombiIndex = 0xdead;
void CKGBankManager::ChangeKarmaPerfForCombi(eSTGCombiBankId bankId, unsigned int index)
{ g_lastCombiBankId = (unsigned int)bankId; g_lastCombiIndex = index; }
static unsigned int g_lastProgBankId = 0xdead, g_lastProgIndex = 0xdead;
void CKGBankManager::ChangeKarmaPerfForProgram(eSTGProgramBankId bankId, unsigned int index)
{ g_lastProgBankId = (unsigned int)bankId; g_lastProgIndex = index; }
static unsigned int g_lastSeqIndex = 0xdead;
void CKGBankManager::ChangeKarmaPerfForSeq(unsigned int index) { g_lastSeqIndex = index; }

void CDrumTrackBankManager::ChangeCombi(eSTGCombiBankId, unsigned int) {}
void CDrumTrackBankManager::ChangeProgram(eSTGProgramBankId, unsigned int) {}
void CDrumTrackBankManager::ChangeSeq(unsigned int) {}
void CSPREngine::ChangePerformance(eSTGMsgPerfType) {}
void CMIDIFlowParamHolder::SetCurrentVoiceMode() {}
void CMIDIFlowParamHolder::ChangePerformance() {}
void CKGRTCHandler::FlashBufferdValue() {}
void CKGRTCHandler::StartBuffering() {}
void CSKMIDIMsgProcessor::KillAllDyingNotes() {}
void CSKMIDIMsgProcessor::LeaveDownloadMode() {}
void CSKMIDIMsgProcessor::StoreDyingNoteInfoForMIDPort(CMIDIMessage *) {}
void CSKMIDIMsgProcessor::StoreDyingNoteInfoForSTG(CMIDIMessage *) {}
void CSKMIDIMsgProcessor::ProcessLocalControlChannelMessage(int, unsigned char, char, char) {}
void CSKMIDIMsgProcessor::ProcessKarmaControllerGeneratedChannelMessage(int, unsigned char, char, char) {}
void CKGMIDIMsgProcessor::KillAllDyingNotes() {}
void CKGMIDIMsgProcessor::ResetKarmaGeneratedCCValue() {}
static CMIDIMessage *g_lastStoreCCMessageArg;
void CKGMIDIMsgProcessor::StoreCCMessage(CMIDIMessage *msg) { g_lastStoreCCMessageArg = msg; }

static int g_curLocLastCallKind; /* 0=none,1=current,2=precount */
static int g_locBar = 5, g_locBeat = 10, g_locC = 4, g_locD = 0;
void CSPRClockHandler::GetCurrentLocation(int *a, int *b, int *c, int *d)
{ g_curLocLastCallKind = 1; *a = g_locBar; *b = g_locBeat; *c = g_locC; *d = g_locD; }
void CSPRClockHandler::GetPrecountLocation(int *a, int *b, int *c, int *d)
{ g_curLocLastCallKind = 2; *a = g_locBar; *b = g_locBeat; *c = g_locC; *d = g_locD; }

static unsigned char g_paramMsgBackingIsThis = 1;
static unsigned int g_paramMsgBackingValue = 0x55;
bool CSKParameterChangeMessage::IsThisParamChage() { return g_paramMsgBackingIsThis != 0; }
unsigned int CSKParameterChangeMessage::GetValue() { return g_paramMsgBackingValue; }

static CSKParameterChangeMessage *g_lastSetValueArg;
void CSPRSysExBufManager::SetValue(CSKParameterChangeMessage *msg) { g_lastSetValueArg = msg; }

/* ==================== mocked free functions ==================== */

extern "C" void SKSTGGate_SendToSTG(const unsigned char *, unsigned short) {}
static int g_sendToMIDIPortCalls;
static unsigned short g_lastSendToMIDIPortLen;
extern "C" void SKSTGGate_SendToMIDIPort(const unsigned char *, unsigned short len)
{ g_sendToMIDIPortCalls++; g_lastSendToMIDIPortLen = len; }
static bool g_vjsccFilterResult = true;
extern "C" bool SKSTGGate_CheckVJSCCToMIDIPortFilter(int, int) { return g_vjsccFilterResult; }
static int g_lastRecStatus = -1, g_lastRecNote = -1, g_lastRecCC = -1, g_lastRecChannel = -1;
extern "C" void SPRMain_RecChannelMessage(int status, int note, int cc, int channel)
{ g_lastRecStatus = status; g_lastRecNote = note; g_lastRecCC = cc; g_lastRecChannel = channel; }
extern "C" unsigned char CAfterTouchConverter_ConvertPostMIDI(int, unsigned char v) { return v; }
extern "C" unsigned char CAfterTouchConverter_ConvertPreMIDI(int, unsigned char v) { return v; }
extern "C" unsigned char CVelocityConverter_ConvertPostMIDI(int, unsigned char v) { return v; }
static bool g_isExclusive = true;
extern "C" bool SPROutGate_IsEnableExclusive() { return g_isExclusive; }
static bool g_autoKindResult = false;
static int g_autoKindOut = 0;
extern "C" char SPROutGate_GetAutomationSysExEventKind(unsigned int, int *out)
{ *out = g_autoKindOut; return g_autoKindResult; }
extern "C" void KGMain_ReceiveParameterChangeMessageFromMIDIPort(unsigned char *) {}
extern "C" void KGMain_ReceiveParameterChangeMessageFromSeqEvent(unsigned char *) {}
extern "C" void KGMain_ReceiveKarmaDisableInputMessage(unsigned char *, int) {}
extern "C" void SPRMain_ReceiveParameterChangeMessageFromMIDIPort(unsigned char *) {}
extern "C" void SPRMain_ReceiveParameterChangeMessageFromSeqEvent(unsigned char *) {}
extern "C" void SPRMain_ReceiveDrumTrackParameterChangeMessageFromMIDIPort(unsigned char *) {}
extern "C" void SPRMain_ReceiveDrumTrackParameterChangeMessageFromSeqEvent(unsigned char *) {}
static int g_recInternalCalls;
extern "C" void SPRMain_RecInternalSysExMessage(unsigned char) { g_recInternalCalls++; }
extern "C" void SPRMain_RecSysExMessageOnAutomationTrack(int, int, int) {}
extern "C" void SPRMain_RecSysExMessageFromMIDIPort(unsigned char) {}

static void reset_all_mocks()
{
	memset(g_bankBuf, 0, sizeof(g_bankBuf));
	memset(g_engineBuf, 0, sizeof(g_engineBuf));
	memset(g_stgMsgProcBuf, 0, sizeof(g_stgMsgProcBuf));
	CKGEngine::ms_poInstance = g_engineBuf;
	CSTGMessageProcessor::sInstance = (CSTGMessageProcessor *)g_stgMsgProcBuf;
	g_changePerformanceCalls = 0;
	g_shouldKeepKarma = true;
	g_clearSchedulerCalls = g_karmaOnCalls = g_karmaOffCalls = g_resetLocalCtrlCalls = 0;
	g_sendShutUpCalls = g_sendCCOffsetBackCalls = 0;
	g_lastBendRangeChannel = -1;
	g_isExclusive = true;
	g_recInternalCalls = 0;
	g_sendToMIDIPortCalls = 0;
}

int main(void)
{
	printf("CSKMIDIMsgHandler/CSKSpecialMsgHandler/CSKSysExMsgHandler known-answer test\n");
	printf("=============================================================================\n");

	/* ---- Structural: virtual dispatch through CSKMIDIMsgHandler* ---- */
	printf("-- single-inheritance dispatch (via CSKMIDIMsgHandler* base pointer) --\n");
	{
		reset_all_mocks();
		CSKSysExMsgHandler sysex;
		CSKMIDIMsgHandler *base = &sysex;
		/* CheckPadsMIDIOutFilter() is CSKMIDIMsgHandler's own base
		 * implementation -- CSKSysExMsgHandler doesn't override it,
		 * so dispatch through the base pointer must still reach it. */
		g_bankBuf[0x97c7ba] = 0;
		g_CKGChordTrigger_ms_bNowGenaratingChordNotes = false;
		check("CSKSysExMsgHandler via CSKMIDIMsgHandler*: CheckPadsMIDIOutFilter()==1",
		      base->CheckPadsMIDIOutFilter(), 1);
	}

	/* ---- Independent-oracle KATs (see oracle_ckg_midi.py) ---- */
	printf("-- CSKMIDIMsgHandler::ConvertPostMIDINote() octave-wrap transpose --\n");
	{
		/* transpose=5, note=60 -> 65 */
		reset_all_mocks();
		CSKMIDIMsgHandler h;
		*(int *)(g_bankBuf + 0x97c74c) = 1;
		g_bankBuf[0x97c744] = 5;
		h.m_status = 0x90; h.m_data1 = 60; h.m_data2 = 0x40; h.m_flags = 0;
		h.ConvertPostMIDINote();
		check("transpose=5 note=60 -> 65", h.m_data1, 65);

		/* transpose=-5, note=2 -> 9 (wraps by +0xc) */
		reset_all_mocks();
		*(int *)(g_bankBuf + 0x97c74c) = 1;
		g_bankBuf[0x97c744] = (unsigned char)-5;
		h.m_status = 0x90; h.m_data1 = 2; h.m_data2 = 0x40; h.m_flags = 0;
		h.ConvertPostMIDINote();
		check("transpose=-5 note=2 -> 9", h.m_data1, 9);

		/* transpose=5, note=125 -> 118 (wraps by -0xc) */
		reset_all_mocks();
		*(int *)(g_bankBuf + 0x97c74c) = 1;
		g_bankBuf[0x97c744] = 5;
		h.m_status = 0x90; h.m_data1 = 125; h.m_data2 = 0x40; h.m_flags = 0;
		h.ConvertPostMIDINote();
		check("transpose=5 note=125 -> 118", h.m_data1, 118);
	}

	printf("-- CSKMIDIMsgHandler::CheckAndGetCorrectCCValue() CSWTCH_42 table --\n");
	{
		CSKMIDIMsgHandler h;
		h.m_status = 0xb0; h.m_data1 = 0x11; h.m_data2 = 0xff;
		check("CC 0x11, val=0xff -> table[0]==0x40", h.CheckAndGetCorrectCCValue(), 0x40);
		h.m_data1 = 0x12;
		check("CC 0x12, val=0xff -> table[1]==0", h.CheckAndGetCorrectCCValue(), 0);
		h.m_data1 = 0x40;
		check("CC 0x40 (out of table range), val=0xff -> 0", h.CheckAndGetCorrectCCValue(), 0);
		h.m_data1 = 0x40; h.m_data2 = 5;
		check("CC 0x40, val=5 (not 0xff sentinel) -> passthrough 5", h.CheckAndGetCorrectCCValue(), 5);
		h.m_status = 0x90; h.m_data1 = 0x11; h.m_data2 = 0xff;
		check("status!=0xb0 -> passthrough (signed char)0xff == -1", h.CheckAndGetCorrectCCValue(), -1);
	}

	printf("-- CSKSpecialMsgHandler::ProcessPitchBendMessage() 2-value transform --\n");
	{
		reset_all_mocks();
		CSKSpecialMsgHandler h;
		h.m_status = 0xe0; h.m_data1 = 0x41; h.m_data2 = 0x3f; h.m_flags = 0x10;
		bool active = h.ProcessPitchBendMessage();
		check("active flag consumed", active, 1);
		check("channel == m_status&0xf == 0", g_lastBendRangeChannel, 0);
		check("arg A = transform(0x41) == -63", (long)(int)g_lastBendRangeA, -63);
		check("arg B = transform(0x3f) == 63", (long)(int)g_lastBendRangeB, 63);

		reset_all_mocks();
		h.m_flags = 0;
		active = h.ProcessPitchBendMessage();
		check("flags&0x10==0 -> inactive, no call", active, 0);
	}

	printf("-- CSKSysExMsgHandler::Analize() SOX/EOX state machine --\n");
	{
		reset_all_mocks();
		CSKSysExMsgHandler h;
		unsigned char seq[] = { 0xf0, 0x42, 0x30, 0x00, 0x12, 0xf7 };
		bool lastResult = false;
		for (unsigned char b : seq)
			lastResult = h.Analize(b);
		check("full bracketed frame: final EOX result==true", lastResult, 1);
		check("m_inSysEx cleared after EOX", h.m_inSysEx, 0);

		reset_all_mocks();
		CSKSysExMsgHandler h2;
		unsigned char seq2[] = { 0xf0, 0x42, 0x90 };
		bool r0 = h2.Analize(seq2[0]);
		bool r1 = h2.Analize(seq2[1]);
		bool r2 = h2.Analize(seq2[2]);
		check("SOX result", r0, 1);
		check("data byte result", r1, 1);
		check("interrupted by new status byte (not EOX) -> false", r2, 0);
		check("m_inSysEx cleared on interruption too", h2.m_inSysEx, 0);
	}

	printf("-- CSKSysExMsgHandler::CopyToBuffer() 0x20 wraparound --\n");
	{
		reset_all_mocks();
		CSKSysExMsgHandler h;
		for (int i = 0; i < 33; i++)
			h.CopyToBuffer((unsigned char)i);
		check("33rd byte wraps m_bufIndex back to 1", h.m_bufIndex, 1);
		check("buffer[0] holds the 33rd (wrapped) byte == 32", h.m_buf[0], 32);
		check("buffer[1] still holds the 2nd byte == 1", h.m_buf[1], 1);
	}

	printf("-- CSKSysExMsgHandler::RecChannelMessageToSequencer() dispatch shape --\n");
	{
		reset_all_mocks();
		CSKMIDIMsgHandler h;
		g_bankBuf[0x97c7ba] = 0;	/* CheckPadsMIDIOutFilter -> gate open */
		h.m_status = 0x90; h.m_data1 = 60; h.m_data2 = 0x40; h.m_flags = 3;
		h.RecChannelMessageToSequencer();
		check("SPRMain_RecChannelMessage status arg", g_lastRecStatus, 0x90);
		check("SPRMain_RecChannelMessage channel arg (status&0xf)", g_lastRecChannel, 0);
	}

	printf("-- CSKSysExMsgHandler::ProcessProgramChangeMessage() bankId/index mapping --\n");
	{
		/* Combi case (sel==0): bankId=m_data1&0x3f, index=m_data2 raw --
		 * verified against CKGBankManager::ChangeKarmaPerfForCombi's
		 * OWN prologue (bounds-checks index<=0x7f, stores bankId
		 * unchecked), not guessed from naming. */
		reset_all_mocks();
		CSKSpecialMsgHandler h;
		h.m_flags = 0x10;
		h.m_data1 = 0x25;	/* sel = 0x25>>6 = 0 (Combi), bankId-part = 0x25&0x3f = 0x25 */
		h.m_data2 = 0x50;	/* index */
		h.ProcessProgramChangeMessage();
		check("Combi: bankId == m_data1&0x3f", (long)g_lastCombiBankId, 0x25);
		check("Combi: index == m_data2 (raw)", (long)g_lastCombiIndex, 0x50);
		check("Combi: ChangePerformance(eSTGMsgPerfType_Combi)", g_lastChangePerfType, eSTGMsgPerfType_Combi);

		reset_all_mocks();
		h.m_flags = 0x10;
		h.m_data1 = 0x65;	/* sel = 0x65>>6 = 1 (Program) */
		h.m_data2 = 0x22;	/* < 0x80 and != 0xfe -> normal path */
		h.ProcessProgramChangeMessage();
		check("Program (normal): bankId == m_data1&0x3f", (long)g_lastProgBankId, 0x25);
		check("Program (normal): index == m_data2 (raw)", (long)g_lastProgIndex, 0x22);

		reset_all_mocks();
		h.m_flags = 0x10;
		h.m_data1 = 0x65;
		h.m_data2 = 0xfe;	/* sentinel path */
		h.ProcessProgramChangeMessage();
		check("Program (sentinel 0xfe): bankId unchanged (data1&0x3f)", (long)g_lastProgBankId, 0x25);
		check("Program (sentinel 0xfe): index forced to 0xfffe", (long)g_lastProgIndex, 0xfffe);

		reset_all_mocks();
		h.m_flags = 0x10;
		h.m_data1 = 0xa5;	/* sel = 0xa5>>6 = 2 (Seq) */
		h.m_data2 = 0x30;
		h.ProcessProgramChangeMessage();
		check("Seq: index == m_data2 (raw)", (long)g_lastSeqIndex, 0x30);
	}

	printf("\n%s\n", g_fail ? "SOME CHECKS FAILED" : "all checks passed");
	return g_fail ? 1 : 0;
}
